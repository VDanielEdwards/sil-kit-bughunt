// Copyright (c) Vector Informatik GmbH. All rights reserved.

#include <cassert>
#include <sstream>

#include "CanController.hpp"
#include "CanControllerProxy.hpp"
#include "CanControllerFacade.hpp"
#include "CanControllerReplay.hpp"
#include "EthController.hpp"
#include "EthControllerProxy.hpp"
#include "EthControllerFacade.hpp"
#include "EthControllerReplay.hpp"
#include "FrController.hpp"
#include "FrControllerProxy.hpp"
#include "LinController.hpp"
#include "LinControllerReplay.hpp"
#include "LinControllerProxy.hpp"
#include "LinControllerFacade.hpp"
#include "InPort.hpp"
#include "InPortReplay.hpp"
#include "OutPort.hpp"
#include "OutPortReplay.hpp"
#include "GenericPublisher.hpp"
#include "GenericPublisherReplay.hpp"
#include "GenericSubscriber.hpp"
#include "GenericSubscriberReplay.hpp"
#include "DataPublisher.hpp"
#include "DataSubscriber.hpp"
#include "DataSubscriberInternal.hpp"
#include "ParticipantController.hpp"
#include "SystemController.hpp"
#include "SystemMonitor.hpp"
#include "LogMsgSender.hpp"
#include "LogMsgReceiver.hpp"
#include "Logger.hpp"
#include "TimeProvider.hpp"
#include "ServiceDiscovery.hpp"

#include "tuple_tools/bind.hpp"
#include "tuple_tools/for_each.hpp"
#include "tuple_tools/predicative_get.hpp"

#include "ib/cfg/string_utils.hpp"
#include "ib/version.hpp"

#include "ComAdapter.hpp"

#include "MessageTracing.hpp" // log tracing
#include "UuidRandom.hpp"

#ifdef SendMessage
#if SendMessage == SendMessageA
#undef SendMessage
#endif
#endif //SendMessage


namespace ib {
namespace mw {

using namespace ib::sim;

namespace tt = util::tuple_tools;

// Anonymous namespace for Helper Traits and Functions
namespace {

template<class T, class U>
struct IsControllerMap : std::false_type {};
template<class T, class U>
struct IsControllerMap<std::unordered_map<std::string, std::unique_ptr<T>>, U> : std::is_base_of<T, U> {};


// Helper to find all the NetworkSimulator configuration blocks,
// which now reside in the participant configuration.
auto FindNetworkSimulators(const cfg::SimulationSetup& simulationSetup) 
    -> std::vector<cfg::NetworkSimulator>
{
    std::vector<cfg::NetworkSimulator> result;
    for (const auto& participant : simulationSetup.participants)
    {
        std::copy(participant.networkSimulators.begin(), participant.networkSimulators.end(),
            std::back_inserter(result));
    }
    return result;
}

template<typename ConfigT>
bool ControllerUsesReplay(const ConfigT& controllerConfig)
{
    return controllerConfig.replay.direction != cfg::Replay::Direction::Undefined
        && !controllerConfig.replay.useTraceSource.empty();
}

} // namespace anonymous

template <class IbConnectionT>
ComAdapter<IbConnectionT>::ComAdapter(cfg::Config config, const std::string& participantName)
    : _config{std::move(config)}
    , _participant{GetParticipantByName(_config, participantName)} // throws if participantName is not found in _config
    , _participantName{participantName}
    , _participantId{hash(_participant.name)}
    , _ibConnection{_config, participantName, _participantId}
{
    // NB: do not create the _logger in the initializer list. If participantName is empty,
    //  this will cause a fairly unintuitive exception in spdlog.
    auto&& participantConfig = get_by_name(_config.simulationSetup.participants, _participantName);
    _logger = std::make_unique<logging::Logger>(_participantName, participantConfig.logger);
    _ibConnection.SetLogger(_logger.get());

    _logger->Info("Creating ComAdapter for Participant {}, IntegrationBus-Version: {} {}, Middleware: {}",
        _participantName, version::String(), version::SprintName(),
        to_string(_config.middlewareConfig.activeMiddleware));
    if (!_config.configFilePath.empty())
        _logger->Info("Using IbConfig: {}", _config.configFilePath);

    //set up default time provider used for controller instantiation
    _timeProvider = std::make_shared<sync::WallclockProvider>(_config.simulationSetup.timeSync.tickPeriod);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::joinIbDomain(uint32_t domainId)
{
    _ibConnection.JoinDomain(domainId);
    onIbDomainJoined();

    _logger->Info("Participant {} has joined the IB-Domain {}", _participantName, domainId);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::onIbDomainJoined()
{
    SetupRemoteLogging();

    //Ensure Service discovery is started
    GetServiceDiscovery()->Initialize();

    // Create the participants trace message sinks as declared in the configuration.
    _traceSinks = tracing::CreateTraceMessageSinks(GetLogger(), _config, _participant);

    if (_participant.participantController.has_value())
    {
        auto* participantController =
            static_cast<sync::ParticipantController*>(GetParticipantController());
        _timeProvider = participantController->GetTimeProvider();
    }
    _logger->Info("Time provider: {}", _timeProvider->TimeProviderName());

    // Enable replaying mechanism.
    const auto& participantConfig = get_by_name(_config.simulationSetup.participants, _participantName);
    if (tracing::HasReplayConfig(participantConfig))
    {
        _replayScheduler = std::make_unique<tracing::ReplayScheduler>(_config,
            participantConfig,
            _config.simulationSetup.timeSync.tickPeriod,
            this,
            _timeProvider.get()
        );
        _logger->Info("Replay Scheduler active.");
    }

    // Ensure shutdowns are cleanly handled.
    auto&& monitor = GetSystemMonitor();
    monitor->RegisterSystemStateHandler([&conn = GetIbConnection()](auto newState)
    {
        if (newState == sync::SystemState::ShuttingDown)
        {
            conn.NotifyShutdown();
        }
    });
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SetupRemoteLogging()
{
    auto* logger = dynamic_cast<logging::Logger*>(_logger.get());
    if (logger)
    {
        if (_participant.logger.logFromRemotes)
        {
            CreateController<logging::LogMsgReceiver>("LogMsgReceiver", logger);
        }

        auto sinkIter = std::find_if(_participant.logger.sinks.begin(), _participant.logger.sinks.end(),
            [](const cfg::Sink& sink) { return sink.type == cfg::Sink::Type::Remote; });

        if (sinkIter != _participant.logger.sinks.end())
        {
            auto&& logMsgSender = CreateController<logging::LogMsgSender>("LogMsgSender");

            logger->RegisterRemoteLogging([logMsgSender](logging::LogMsg logMsg) {

                logMsgSender->SendLogMsg(std::move(logMsg));

            });
        }
    }
    else
    {
        _logger->Warn("Failed to setup remote logging. Participant {} will not send and receive remote logs.", _participantName);
    }
}

template<class IbConnectionT>
inline void ComAdapter<IbConnectionT>::SetTimeProvider(sync::ITimeProvider* newClock)
{
    // register the time provider with all already instantiated controllers
    auto setTimeProvider = [this, newClock](auto& controllers) {
        for (auto& controller: controllers)
        {
            auto* ctl = dynamic_cast<ib::mw::sync::ITimeConsumer*>(controller.second.get());
            if (ctl)
            {
                ctl->SetTimeProvider(newClock);
            }
        }
    };
    tt::for_each(_controllers, setTimeProvider);
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateCanController(const std::string& canonicalName) -> can::ICanController*
{
    auto&& config = get_by_name(_participant.canControllers, canonicalName);

    // TODO replay is currently not possible - need concept!!
    return CreateControllerForLink<can::CanControllerFacade>(config, _timeProvider.get());

    //if (ControllerUsesNetworkSimulator(config.name))
    //{
    //    return CreateControllerForLink<can::CanControllerProxy>(config);
    //}
    //else if (ControllerUsesReplay(config))
    //{
    //    return CreateControllerForLink<can::CanControllerReplay>(config, config, _timeProvider.get());
    //}
    //else
    //{
    //    return CreateControllerForLink<can::CanController>(config, _timeProvider.get());
    //}
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateEthController(const std::string& canonicalName) -> eth::IEthController*
{
    auto&& config = get_by_name(_participant.ethernetControllers, canonicalName);

    // TODO replay is currently not possible - need concept!!
    return CreateControllerForLink<eth::EthControllerFacade>(config, config, _timeProvider.get());

    //if (ControllerUsesNetworkSimulator(config.name))
    //{
    //    return CreateControllerForLink<eth::EthControllerProxy>(config, config);
    //}
    //else if (ControllerUsesReplay(config))
    //{
    //    return CreateControllerForLink<eth::EthControllerReplay>(config, config, _timeProvider.get());
    //}
    //else
    //{
    //    return CreateControllerForLink<eth::EthController>(config, config, _timeProvider.get());
    //}
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateFlexrayController(const std::string& canonicalName) -> sim::fr::IFrController*
{
    auto&& config = get_by_name(_participant.flexrayControllers, canonicalName);
    if (ControllerUsesNetworkSimulator(config.name))
    {
        return CreateControllerForLink<fr::FrControllerProxy>(config);
    }
    else
    {
        return CreateControllerForLink<fr::FrController>(config, _timeProvider.get());
    }
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateLinController(const std::string& canonicalName) -> lin::ILinController*
{
    auto&& config = get_by_name(_participant.linControllers, canonicalName);
    // TODO replay is currently not possible - need concept!!
    return CreateControllerForLink<lin::LinControllerFacade>(config, _timeProvider.get());
    
    //if (ControllerUsesNetworkSimulator(config.name))
    //{
    //    return CreateControllerForLink<lin::LinControllerProxy>(config);
    //}
    //else if (ControllerUsesReplay(config))
    //{
    //    return CreateControllerForLink<lin::LinControllerReplay>(config, config, _timeProvider.get());
    //}
    //else
    //{
    //    return CreateControllerForLink<lin::LinController>(config, _timeProvider.get());
    //}
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateAnalogIn(const std::string& canonicalName) -> sim::io::IAnalogInPort*
{
    auto&& config = get_by_name(_participant.analogIoPorts, canonicalName);
    return CreateInPort<io::AnalogIoMessage>(config);
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateDigitalIn(const std::string& canonicalName) -> sim::io::IDigitalInPort*
{
    auto&& config = get_by_name(_participant.digitalIoPorts, canonicalName);
    return CreateInPort<io::DigitalIoMessage>(config);
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreatePwmIn(const std::string& canonicalName) -> sim::io::IPwmInPort*
{
    auto&& config = get_by_name(_participant.pwmPorts, canonicalName);
    return CreateInPort<io::PwmIoMessage>(config);
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreatePatternIn(const std::string& canonicalName) -> sim::io::IPatternInPort*
{
    auto&& config = get_by_name(_participant.patternPorts, canonicalName);
    return CreateInPort<io::PatternIoMessage>(config);
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateAnalogOut(const std::string& canonicalName) -> sim::io::IAnalogOutPort*
{
    auto&& config = get_by_name(_participant.analogIoPorts, canonicalName);
    return CreateOutPort<io::AnalogIoMessage>(config);
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateDigitalOut(const std::string& canonicalName) -> sim::io::IDigitalOutPort*
{
    auto&& config = get_by_name(_participant.digitalIoPorts, canonicalName);
    return CreateOutPort<io::DigitalIoMessage>(config);
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreatePwmOut(const std::string& canonicalName) -> sim::io::IPwmOutPort*
{
    auto&& config = get_by_name(_participant.pwmPorts, canonicalName);
    return CreateOutPort<io::PwmIoMessage>(config);
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreatePatternOut(const std::string& canonicalName) -> sim::io::IPatternOutPort*
{
    auto&& config = get_by_name(_participant.patternPorts, canonicalName);
    return CreateOutPort<io::PatternIoMessage>(config);
}

template <class IbConnectionT>
template <class MsgT, class ConfigT>
auto ComAdapter<IbConnectionT>::CreateInPort(const ConfigT& config) -> io::IInPort<MsgT>*
{
    if (config.direction != cfg::PortDirection::In)
        throw cfg::Misconfiguration("Invalid port direction!");

    if (ControllerUsesReplay(config))
    {
        return CreateControllerForLink<io::InPortReplay<MsgT>>(config, config, _timeProvider.get());
    }
    else
    {
        return CreateControllerForLink<io::InPort<MsgT>>(config, config, _timeProvider.get());
    }
}

template <class IbConnectionT>
template <class MsgT, class ConfigT>
auto ComAdapter<IbConnectionT>::CreateOutPort(const ConfigT& config) -> io::IOutPort<MsgT>*
{
    if (config.direction != cfg::PortDirection::Out)
        throw cfg::Misconfiguration("Invalid port direction!");

    if (ControllerUsesReplay(config))
    {
        auto port = CreateControllerForLink<io::OutPortReplay<MsgT>>(config, config, _timeProvider.get());
        port->Write(config.initvalue, std::chrono::nanoseconds{0});
        return port;
    }
    else
    {
        auto port = CreateControllerForLink<io::OutPort<MsgT>>(config, config, _timeProvider.get());
        port->Write(config.initvalue, std::chrono::nanoseconds{0});
        return port;
    }
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateDataSubscriberInternal(const std::string& topic, 
    const std::string& linkName, const sim::data::DataExchangeFormat& dataExchangeFormat, 
    sim::data::CallbackExchangeFormatT callback) -> sim::data::IDataSubscriber*
{
    _logger->Trace("Creating internal subscriber for topic={}, linkName={}", topic, linkName);

    // Link config
    cfg::Link link;
    link.id = static_cast<int16_t>(hash(linkName));
    link.name = linkName;
    link.type = cfg::Link::Type::DataMessage;

    // Controller config
    cfg::DataPort config;
    config.linkId = link.id;
    config.name = topic;
    config.dataExchangeFormat = dataExchangeFormat;
    return CreateController<sim::data::DataSubscriberInternal>(link, config.name, config,
        _timeProvider.get(), callback);

}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateDataPublisher(const std::string& canonicalName,
    const ib::sim::data::DataExchangeFormat& dataExchangeFormat,
    size_t history) -> sim::data::IDataPublisher*
{
    if (history > 1)
    {
        throw cfg::Misconfiguration("DataPublishers do not support history > 1.");
    }

    ib::cfg::DataPort config = get_by_name(_participant.dataPublishers, canonicalName);
    config.dataExchangeFormat = dataExchangeFormat;
    config.history = history;
    config.pubUUID = util::uuid::to_string(util::uuid::generate());

    if (ControllerUsesReplay(config))
    {
        throw cfg::Misconfiguration("Replay is not supported for DataPublisher/DataSubscriber.");
        return nullptr;
    }
    else
    {
        // This controller<DataPublisher> is for the publisher announcement with linkName = topic
        auto controllerForAnnouncement =
            CreateControllerForLink<sim::data::DataPublisher>(config, config, _timeProvider.get());

        // Another controller<DataPublisher> for data publication with linkName = UUID is needed
        // as the architecture doesn't allow multiple links on a single controller.
        cfg::Link link;
        link.id = -1;
        link.name = config.pubUUID;
        link.type = cfg::Link::Type::DataMessage;
        link.historyLength = history;

        ib::cfg::DataPort configForPublishController;
        configForPublishController.dataExchangeFormat.mimeType = config.dataExchangeFormat.mimeType;
        configForPublishController.history = config.history;
        configForPublishController.name = config.pubUUID;
        configForPublishController.pubUUID = config.pubUUID;

        auto controllerForPublish = CreateController<ib::sim::data::DataPublisher>(link,
            configForPublishController.name, configForPublishController,
            _timeProvider.get());
        // Now that we got both controllers, we announce the publisher
        controllerForAnnouncement->SendPublisherAnnouncement();
        // The application code is only interested in publication
        return controllerForPublish;
    }
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateDataSubscriber(const std::string& canonicalName,
    const ib::sim::data::DataExchangeFormat& dataExchangeFormat, ib::sim::data::CallbackExchangeFormatT callback)
    -> sim::data::IDataSubscriber*
{
    ib::cfg::DataPort config = get_by_name(_participant.dataSubscribers, canonicalName);
    config.dataExchangeFormat = dataExchangeFormat;

    if (ControllerUsesReplay(config))
    {
        throw cfg::Misconfiguration("Replay is not supported for DataPublisher/DataSubscriber.");
        return nullptr;
    }
    else
    {
        auto controller = CreateControllerForLink<sim::data::DataSubscriber>(config, config, _timeProvider.get(), callback);
        return controller;
    }
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateGenericPublisher(const std::string& canonicalName) -> sim::generic::IGenericPublisher*
{
    auto&& config = get_by_name(_participant.genericPublishers, canonicalName);
    if (ControllerUsesReplay(config))
    {
        return CreateControllerForLink<sim::generic::GenericPublisherReplay>(config, config, _timeProvider.get());
    }
    else
    {
        return CreateControllerForLink<sim::generic::GenericPublisher>(config, config, _timeProvider.get());
    }
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::CreateGenericSubscriber(const std::string& canonicalName) -> sim::generic::IGenericSubscriber*
{
    auto&& config = get_by_name(_participant.genericSubscribers, canonicalName);
    if (ControllerUsesReplay(config))
    {
        return CreateControllerForLink<sim::generic::GenericSubscriberReplay>(config, config, _timeProvider.get());
    }
    else
    {
        return CreateControllerForLink<sim::generic::GenericSubscriber>(config, config, _timeProvider.get());
    }
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::GetParticipantController() -> sync::IParticipantController*
{
    auto* controller = GetController<sync::ParticipantController>("default", "ParticipantController");
    if (!controller)
    {
        controller = CreateController<sync::ParticipantController>("ParticipantController", _config.simulationSetup, _participant);
    }

    return controller;
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::GetSystemMonitor() -> sync::ISystemMonitor*
{
    auto* controller = GetController<sync::SystemMonitor>("default", "SystemMonitor");
    if (!controller)
    {
        controller = CreateController<sync::SystemMonitor>("SystemMonitor", _config.simulationSetup);
    }
    return controller;
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::GetServiceDiscovery() -> service::IServiceDiscovery*
{
    auto* controller = GetController<service::ServiceDiscovery>("default", "ServiceDiscovery");
    if (!controller)
    {
        controller = CreateController<service::ServiceDiscovery>("ServiceDiscovery", _participantName );
    }
    return controller;
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::GetSystemController() -> sync::ISystemController*
{
    auto* controller = GetController<sync::SystemController>("default", "SystemController");
    if (!controller)
    {
        return CreateController<sync::SystemController>("SystemController" );
    }
    return controller;
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::GetLogger() -> logging::ILogger*
{
    return _logger.get();
}


template <class IbConnectionT>
void ComAdapter<IbConnectionT>::RegisterCanSimulator(can::IIbToCanSimulator* busSim)
{
    RegisterSimulator(busSim, cfg::Link::Type::CAN);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::RegisterEthSimulator(sim::eth::IIbToEthSimulator* busSim)
{
    RegisterSimulator(busSim, cfg::Link::Type::Ethernet);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::RegisterFlexraySimulator(sim::fr::IIbToFrBusSimulator* busSim)
{
    RegisterSimulator(busSim, cfg::Link::Type::FlexRay);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::RegisterLinSimulator(sim::lin::IIbToLinSimulator* busSim)
{
    RegisterSimulator(busSim, cfg::Link::Type::LIN);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const can::CanMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, can::CanMessage&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const can::CanTransmitAcknowledge& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const can::CanControllerStatus& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const can::CanConfigureBaudrate& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const can::CanSetControllerMode& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const eth::EthMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, eth::EthMessage&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const eth::EthTransmitAcknowledge& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const eth::EthStatus& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const eth::EthSetMode& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::FrMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, sim::fr::FrMessage&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::FrMessageAck& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, sim::fr::FrMessageAck&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::FrSymbol& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::FrSymbolAck& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::CycleStart& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::HostCommand& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::ControllerConfig& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::TxBufferConfigUpdate& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::TxBufferUpdate& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::ControllerStatus& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::fr::PocStatus& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::lin::SendFrameRequest& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::lin::SendFrameHeaderRequest& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::lin::Transmission& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::lin::WakeupPulse& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::lin::ControllerConfig& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::lin::ControllerStatusUpdate& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::lin::FrameResponseUpdate& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::io::AnalogIoMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::io::DigitalIoMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::io::PatternIoMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, sim::io::PatternIoMessage&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::io::PwmIoMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::generic::GenericMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, sim::generic::GenericMessage&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::data::DataMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, sim::data::DataMessage&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sim::data::PublisherAnnouncement& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, sim::data::PublisherAnnouncement&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sync::NextSimTask& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sync::ParticipantStatus& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sync::ParticipantCommand& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const sync::SystemCommand& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const logging::LogMsg& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, logging::LogMsg&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const service::ServiceAnnouncement& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const service::ServiceDiscoveryEvent& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
template <typename IbMessageT>
void ComAdapter<IbConnectionT>::SendIbMessageImpl(const IIbServiceEndpoint* from, IbMessageT&& msg)
{
    TraceTx(_logger.get(), from, msg);
    _ibConnection.SendIbMessage(from, std::forward<IbMessageT>(msg));
}

// targeted messaging
template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const can::CanMessage& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, can::CanMessage&& msg)
{
    SendIbMessageImpl(from, targetParticipantName, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const can::CanTransmitAcknowledge& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const can::CanControllerStatus& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const can::CanConfigureBaudrate& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const can::CanSetControllerMode& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const eth::EthMessage& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, eth::EthMessage&& msg)
{
    SendIbMessageImpl(from, targetParticipantName, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const eth::EthTransmitAcknowledge& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const eth::EthStatus& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const eth::EthSetMode& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::FrMessage& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, sim::fr::FrMessage&& msg)
{
    SendIbMessageImpl(from, targetParticipantName, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::FrMessageAck& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, sim::fr::FrMessageAck&& msg)
{
    SendIbMessageImpl(from, targetParticipantName, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::FrSymbol& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::FrSymbolAck& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::CycleStart& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::HostCommand& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::ControllerConfig& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::TxBufferConfigUpdate& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::TxBufferUpdate& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::ControllerStatus& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::fr::PocStatus& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::lin::SendFrameRequest& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::lin::SendFrameHeaderRequest& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::lin::Transmission& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::lin::WakeupPulse& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::lin::ControllerConfig& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::lin::ControllerStatusUpdate& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::lin::FrameResponseUpdate& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::io::AnalogIoMessage& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::io::DigitalIoMessage& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::io::PatternIoMessage& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, sim::io::PatternIoMessage&& msg)
{
    SendIbMessageImpl(from, targetParticipantName, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::io::PwmIoMessage& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::generic::GenericMessage& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, sim::generic::GenericMessage&& msg)
{
    SendIbMessageImpl(from, targetParticipantName, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::data::DataMessage& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, sim::data::DataMessage&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sim::data::PublisherAnnouncement& msg)
{
    SendIbMessageImpl(from, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, sim::data::PublisherAnnouncement&& msg)
{
    SendIbMessageImpl(from, std::move(msg));
}


template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sync::NextSimTask& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sync::ParticipantStatus& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sync::ParticipantCommand& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const sync::SystemCommand& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const logging::LogMsg& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, logging::LogMsg&& msg)
{
    SendIbMessageImpl(from, targetParticipantName, std::move(msg));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const service::ServiceAnnouncement& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::SendIbMessage(const IIbServiceEndpoint* from, const std::string& targetParticipantName, const service::ServiceDiscoveryEvent& msg)
{
    SendIbMessageImpl(from, targetParticipantName, msg);
}

template <class IbConnectionT>
template <typename IbMessageT>
void ComAdapter<IbConnectionT>::SendIbMessageImpl(const IIbServiceEndpoint* from, const std::string& targetParticipantName, IbMessageT&& msg)
{
    TraceTx(_logger.get(), from, msg);
    _ibConnection.SendIbMessage(from, targetParticipantName, std::forward<IbMessageT>(msg));
}


template <class IbConnectionT>
template <class ControllerT>
auto ComAdapter<IbConnectionT>::GetController(const std::string& linkName, const std::string& serviceName) -> ControllerT*
{
    auto&& controllerMap = tt::predicative_get<tt::rbind<IsControllerMap, ControllerT>::template type>(_controllers);
    const auto&& qualifiedName = linkName + "/" + serviceName;
    if (controllerMap.count(qualifiedName))
    {
        return static_cast<ControllerT*>(controllerMap.at(qualifiedName).get());
    }
    else
    {
        return nullptr;
    }
}

template <class IbConnectionT>
template<class ControllerT, typename... Arg>
auto ComAdapter<IbConnectionT>::CreateController(const std::string& serviceName,  Arg&&... arg) -> ControllerT*
{
    //NB internal services have hard-coded endpoint  Ids but no Link configs. so provide one
    cfg::Link link{};
    link.id = -1;
    link.name = "default";
    link.type = cfg::Link::Type::Undefined; // internal usage, normally "default"
    return CreateController<ControllerT>(link, serviceName, std::forward<Arg>(arg)...);
}

template <class IbConnectionT>
template <class ControllerT, typename... Arg>
auto ComAdapter<IbConnectionT>::CreateController(const cfg::Link& link, const std::string& serviceName, Arg&&... arg) -> ControllerT*
{
    auto&& controllerMap = tt::predicative_get<tt::rbind<IsControllerMap, ControllerT>::template type>(_controllers);
    auto controller = std::make_unique<ControllerT>(this, std::forward<Arg>(arg)...);
    auto* controllerPtr = controller.get();

    auto myEndpoint = _localEndpointId++;
    controller->SetEndpointAddress({ _participantId, myEndpoint });

    auto descriptor = ServiceDescriptor{};
    descriptor.linkName = link.name;
    descriptor.participantName = _participantName;
    descriptor.serviceName = serviceName;
    descriptor.type = link.type;
    descriptor.legacyEpa = controller->EndpointAddress();
    descriptor.serviceId = myEndpoint;
    controller->SetServiceDescriptor(descriptor);

    _ibConnection.RegisterIbService(link.name, myEndpoint, controllerPtr);
    _ibConnection.SetHistoryLengthForLink(link.name, link.historyLength, controllerPtr);
    const auto qualifiedName = link.name + "/" + serviceName;
    controllerMap[qualifiedName] = std::move(controller);

    GetServiceDiscovery()->NotifyServiceCreated(controllerPtr->GetServiceDescriptor());

    return controllerPtr;
}

template <class IbConnectionT>
auto ComAdapter<IbConnectionT>::GetLinkById(int16_t linkId) -> cfg::Link&
{
    for (auto&& link : _config.simulationSetup.links)
    {
        if (link.id == linkId)
            return link;
    }

    throw cfg::Misconfiguration("Invalid linkId " + std::to_string(linkId));
}

template <class IbConnectionT>
template <class ConfigT>
void ComAdapter<IbConnectionT>::AddTraceSinksToSource(extensions::ITraceMessageSource* traceSource, ConfigT config)
{
    if (config.useTraceSinks.empty())
    {
        GetLogger()->Debug("Tracer on {}/{} not enabled, skipping", _participant.name, config.name);
        return;
    }
    auto findSinkByName = [this](const auto& name)
    {
       return std::find_if(_traceSinks.begin(), _traceSinks.end(),
            [&name](const auto& sinkPtr) {
                return sinkPtr->Name() == name;
            });
    };

    for (const auto& sinkName : config.useTraceSinks)
    {
        auto sinkIter = findSinkByName(sinkName);
        if (sinkIter == _traceSinks.end())
        {
            std::stringstream ss;
            ss << "Controller " << config.name << " refers to non-existing sink "
                << sinkName;

            GetLogger()->Error(ss.str());
            throw cfg::Misconfiguration(ss.str());
        }
        traceSource->AddSink((*sinkIter).get());
    }
}

template <class IbConnectionT>
template <class ControllerT, class ConfigT, typename... Arg>
auto ComAdapter<IbConnectionT>::CreateControllerForLink(const ConfigT& config, Arg&&... arg) -> ControllerT*
{
    auto&& linkCfg = GetLinkById(config.linkId);

    auto* controllerPtr = GetController<ControllerT>(linkCfg.name, config.name);
    if (controllerPtr != nullptr)
    {
        // We cache the controller and return it here.
        return controllerPtr;
    }

    // Create a new controller, and configure tracing if applicable
    auto* controller = CreateController<ControllerT>(linkCfg, config.name, std::forward<Arg>(arg)...);
    auto* traceSource = dynamic_cast<extensions::ITraceMessageSource*>(controller);
    if (traceSource)
    {
        AddTraceSinksToSource(traceSource, config);
    }

    return controller;
}


template <class IbConnectionT>
template <class IIbToSimulatorT>
void ComAdapter<IbConnectionT>::RegisterSimulator(IIbToSimulatorT* busSim, cfg::Link::Type linkType)
{
    auto&& simulator = std::get<IIbToSimulatorT*>(_simulators);
    if (simulator)
    {
        _logger->Error("A {} is already registered", typeid(IIbToSimulatorT).name());
        return;
    }

    struct ServiceCfg
    {
        std::string participantName;
        std::string serviceName;
        EndpointId id;
    };
    std::unordered_map<std::string, ServiceCfg> endpointMap;
    auto addToEndpointMap = [&endpointMap](auto&& participantName, auto&& controllerConfigs)
    {
        for (auto&& cfg : controllerConfigs)
        {
            std::string qualifiedName = participantName + "/" + cfg.name;
            ServiceCfg helper{};
            helper.id = cfg.endpointId;
            helper.participantName = participantName;
            helper.serviceName = cfg.name;
            endpointMap[qualifiedName] = helper;
        }
    };

    for (auto&& participant : _config.simulationSetup.participants)
    {
        addToEndpointMap(participant.name, participant.canControllers);
        addToEndpointMap(participant.name, participant.linControllers);
        addToEndpointMap(participant.name, participant.ethernetControllers);
        addToEndpointMap(participant.name, participant.flexrayControllers);
    }
    for (auto&& ethSwitch : _config.simulationSetup.switches)
    {
        addToEndpointMap(ethSwitch.name, ethSwitch.ports);
    }

    // get_by_name throws if the current node is not configured as a network simulator.
    for (const auto& simulatorConfig : _participant.networkSimulators)
    {
        for (auto&& linkName : simulatorConfig.simulatedLinks)
        {
            auto&& linkConfig = get_by_name(_config.simulationSetup.links, linkName);

            if (linkConfig.type != linkType)
                continue;

            for (auto&& endpointName : linkConfig.endpoints)
            {
                try
                {
                    auto proxyEndpoint = endpointMap.at(endpointName);
                    // NB: We need to set the service id -- VIBE-NetSim implements the IIbServiceEndpoint.
                    // We need to register all simulated controllers here, so the connection
                    // can build internal data structures.
                    auto& serviceEndpoint = dynamic_cast<mw::IIbServiceEndpoint&>(*busSim);
                    auto id = ServiceDescriptor{};
                    id.linkName = linkName;
                    id.participantName = proxyEndpoint.participantName;
                    id.serviceName = proxyEndpoint.serviceName;
                    id.type = linkType;
                    id.legacyEpa.participant = _participantId;
                    id.legacyEpa.endpoint = proxyEndpoint.id;
                    serviceEndpoint.SetServiceDescriptor(id);
  
                    _ibConnection.RegisterIbService(linkName, proxyEndpoint.id, busSim);

                }
                catch (const std::exception& e)
                {
                    _logger->Error("Cannot register simulator topics for link \"{}\": {}", linkName, e.what());
                    continue;
                }
            }
        }
        // register each simulator as trace source
        auto* traceSource = dynamic_cast<extensions::ITraceMessageSource*>(busSim);
        if (traceSource)
        {
            AddTraceSinksToSource(traceSource, simulatorConfig);
        }
    }

    // register the network simulator for replay
    if (_replayScheduler && tracing::HasReplayConfig(_participant))
    {
        try
        {
            _replayScheduler->ConfigureNetworkSimulators(_config, _participant,
                dynamic_cast<tracing::IReplayDataController&>(*busSim));
        }
        catch (const std::exception& e)
        {
            _logger->Error("Cannot configure replaying on network simulator: {}", e.what());
        }
    }

    simulator = busSim;
}

template <class IbConnectionT>
bool ComAdapter<IbConnectionT>::ControllerUsesNetworkSimulator(const std::string& controllerName) const
{
    auto endpointName = _participantName + "/" + controllerName;
    const auto networkSimulators = FindNetworkSimulators(_config.simulationSetup);
  
    if (networkSimulators.empty())
    {
        // no participant with a network simulators present in config
        return false;
    }

    for (auto&& link : _config.simulationSetup.links)
    {
        auto endpointIter = std::find(link.endpoints.begin(), link.endpoints.end(), endpointName);

        if (endpointIter == link.endpoints.end())
            continue;

        //check if the link is a network simulator's simulated link
        for (const auto& simulator : networkSimulators)
        {
            auto linkIter = std::find(simulator.simulatedLinks.begin(), simulator.simulatedLinks.end(), link.name);
            if (linkIter != simulator.simulatedLinks.end())
                return true;
        }
    }

    return false;
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::OnAllMessagesDelivered(std::function<void()> callback)
{
    _ibConnection.OnAllMessagesDelivered(std::move(callback));
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::FlushSendBuffers()
{
    _ibConnection.FlushSendBuffers();
}

template <class IbConnectionT>
void ComAdapter<IbConnectionT>::ExecuteDeferred(std::function<void()> callback)
{
    _ibConnection.ExecuteDeferred(std::move(callback));
}

} // namespace mw
} // namespace ib
