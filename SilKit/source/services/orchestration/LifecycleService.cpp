/* Copyright (c) 2022 Vector Informatik GmbH

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <future>

#include "silkit/services/logging/ILogger.hpp"
#include "silkit/services/orchestration/ISystemMonitor.hpp"
#include "silkit/services/orchestration/string_utils.hpp"

#include "LifecycleService.hpp"
#include "TimeSyncService.hpp"
#include "IServiceDiscovery.hpp"
#include "LifecycleManagement.hpp"
#include "SetThreadName.hpp"

using namespace std::chrono_literals;

namespace SilKit {
namespace Services {
namespace Orchestration {
LifecycleService::LifecycleService(Core::IParticipantInternal* participant,
                                   const Config::HealthCheck& healthCheckConfig)
    : _participant{participant}
    , _logger{participant->GetLogger()}
    ,_lifecycleManagement{participant->GetLogger(), this}
{
    _timeSyncService = _participant->CreateTimeSyncService(this);

    _status.participantName = _participant->GetParticipantName();

    // TODO healthCheckConfig needed?
    (void)healthCheckConfig;
}

 LifecycleService::~LifecycleService()
{
     if(_commReadyHandlerThread.joinable())
     {
         _logger->Debug("LifecycleService::~LifecycleService: joining commReadyHandlerThread");
         _commReadyHandlerThread.join();
     }
}

void LifecycleService::SetCommunicationReadyHandler(CommunicationReadyHandler handler)
{
    _commReadyHandlerIsAsync = false;
    _commReadyHandler = std::move(handler);
}

void LifecycleService::SetCommunicationReadyHandlerAsync(CommunicationReadyHandler handler)
{
    _commReadyHandlerIsAsync = true;
    _commReadyHandler = std::move(handler);
}
void LifecycleService::CompleteCommunicationReadyHandlerAsync()
{
    _logger->Debug("LifecycleService::CompleteCommunicationReadyHandler: enter");
    const auto myself = std::this_thread::get_id();
    if(_commReadyHandlerThread.joinable()
        && (myself  != _commReadyHandlerThread.get_id()))
    {
        _logger->Debug("LifecycleService::CompleteCommunicationReadyHandler: joining commReadyHandlerThread");
        _commReadyHandlerThread.join();
        _commReadyHandlerThread = {};
        // async handler is finished, now continue to the Running state without triggering the CommunicationReadyHandler again
        _lifecycleManagement.SetState(_lifecycleManagement.GetReadyToRunState(),
            "LifecycleService::CompleteCommunicationReadyHandler: triggering communication ready transition.");
    }
}


void LifecycleService::SetStartingHandler(StartingHandler handler)
{
    _startingHandler = std::move(handler);
}

void LifecycleService::SetStopHandler(StopHandler handler)
{
    _stopHandler = std::move(handler);
}

void LifecycleService::SetShutdownHandler(ShutdownHandler handler)
{
    _shutdownHandler = std::move(handler);
}

auto LifecycleService::StartLifecycle(bool hasCoordinatedSimulationStart, bool hasCoordinatedSimulationStop)
    -> std::future<ParticipantState>
{
    if (_timeSyncActive)
    {
        _timeSyncService->ConfigureTimeProvider(TimeProviderKind::SyncTime);
    }
    else
    {
        _timeSyncService->ConfigureTimeProvider(TimeProviderKind::NoSync);
    }
    _timeSyncService->InitializeTimeSyncPolicy(_timeSyncActive);

    _hasCoordinatedSimulationStart = hasCoordinatedSimulationStart;
    _hasCoordinatedSimulationStop = hasCoordinatedSimulationStop;

    // Update ServiceDescriptor
    _serviceDescriptor.SetSupplementalDataItem(SilKit::Core::Discovery::lifecycleHasCoordinatedStart,
                                               std::to_string(hasCoordinatedSimulationStart));
    _serviceDescriptor.SetSupplementalDataItem(SilKit::Core::Discovery::lifecycleHasCoordinatedStop,
                                               std::to_string(hasCoordinatedSimulationStop));

    // Publish services
    auto serviceDiscovery = _participant->GetServiceDiscovery();
    serviceDiscovery->NotifyServiceCreated(_serviceDescriptor);
    serviceDiscovery->NotifyServiceCreated(_timeSyncService->GetServiceDescriptor());

    _participant->GetSystemMonitor()->AddSystemStateHandler([&](auto systemState) {
        this->NewSystemState(systemState);
    });

    _isRunning = true;
    _lifecycleManagement.InitLifecycleManagement("LifecycleService::StartLifecycle... was called.");
    if (!hasCoordinatedSimulationStart)
    {
        // Skip state guarantees if start is uncoordinated
        _lifecycleManagement.SkipSetupPhase(
            "LifecycleService::StartLifecycle... was called without start coordination.");
    }
    return _finalStatePromise.get_future();
}

auto LifecycleService::StartLifecycle(LifecycleConfiguration startConfiguration) -> std::future<ParticipantState>
{
    return StartLifecycle(startConfiguration.coordinatedStart,
            startConfiguration.coordinatedStop);
}

void LifecycleService::ReportError(std::string errorMsg)
{
    _logger->Error(errorMsg);

    // If the lifecycle is not executing, log message and skip further error handling
    if (State() == ParticipantState::Invalid)
    {
        return;
    }

    if (State() == ParticipantState::Shutdown)
    {
        _logger->Warn("LifecycleService::ReportError() was called in terminal state ParticipantState::Shutdown; "
                      "transition to ParticipantState::Error is ignored.");
        return;
    }

    _lifecycleManagement.Error(std::move(errorMsg));
}

void LifecycleService::Pause(std::string reason)
{
    if (State() != ParticipantState::Running)
    {
        const std::string errorMessage{"TimeSyncService::Pause() was called in state ParticipantState::"
                                       + to_string(State())};
        ReportError(errorMessage);
        throw std::runtime_error(errorMessage);
    }
    _pauseDonePromise = decltype(_pauseDonePromise){};
    _timeSyncService->SetPaused(_pauseDonePromise.get_future());
    _lifecycleManagement.Pause(reason);
}

void LifecycleService::Continue()
{
    if (State() != ParticipantState::Paused)
    {
        const std::string errorMessage{"TimeSyncService::Continue() was called in state ParticipantState::"
                                       + to_string(State())};
        ReportError(errorMessage);
        throw std::runtime_error(errorMessage);
    }

    _lifecycleManagement.Continue("Pause finished");
    _pauseDonePromise.set_value();
}

void LifecycleService::Stop(std::string reason)
{
    _lifecycleManagement.Stop(reason);

    if (!_hasCoordinatedSimulationStop) 
    {
        Shutdown("Shutdown after LifecycleService::Stop without stop coordination.");
    }
}

void LifecycleService::Shutdown(std::string reason)
{
    auto success = _lifecycleManagement.Shutdown(reason);
    if (success)
    {
        try
        {
            _finalStatePromise.set_value(State());
        }
        catch (const std::future_error&)
        {
            // NOP - received shutdown multiple times
        }
    }
    else
    {
        _logger->Warn("lifecycle failed to shut down correctly - original shutdown reason was '{}'.",
                      std::move(reason));
    }
}

void LifecycleService::Restart(std::string reason)
{
    _lifecycleManagement.Restart(reason);

    if (!_hasCoordinatedSimulationStart)
    {
        _lifecycleManagement.Run("LifecycleService::Restart() was called without start coordination.");
    }
}

bool LifecycleService::TriggerCommunicationReadyHandler(std::string)
{
    if(_commReadyHandler)
    {
        if(_commReadyHandlerIsAsync)
        {
            if(_commReadyHandlerThread.joinable())
            {
                _logger->Debug("LifecycleServiec::TriggerCommunicationReadyHandler:"
                    " commReadyHandlerThread is already joinable. Ensure you called CompleteCommunicationReadyHandler.");
                return false;
            }

            _commReadyHandlerThread = std::thread{[this]{
                Util::SetThreadName("SKCommReadyHdlr");
                _commReadyHandler();
            }};
            return false;
        }
        else
        {
            _commReadyHandler();
        }
    }
    return true;
}

void LifecycleService::TriggerStartingHandler(std::string)
{
    if (_startingHandler)
    {
        _startingHandler();
    }
}

void LifecycleService::TriggerStopHandler(std::string)
{
    if (_stopHandler)
    {
        _stopHandler();
    }
}

void LifecycleService::TriggerShutdownHandler(std::string)
{
    if (_shutdownHandler)
    {
        _shutdownHandler();
    }
}

void LifecycleService::AbortSimulation(std::string reason)
{
    auto success = _lifecycleManagement.AbortSimulation(reason);
    if (success)
    {
        try
        {
            _finalStatePromise.set_value(State());
        }
        catch (const std::future_error&)
        {
            // NOP - received shutdown multiple times
        }
    }
}

auto LifecycleService::State() const -> ParticipantState
{
    return _status.state;
}

auto LifecycleService::Status() const -> const ParticipantStatus&
{
    return _status;
}

void LifecycleService::ReceiveMsg(const IServiceEndpoint* /*from*/, const ParticipantCommand& command)
{
    if (command.participant != _serviceDescriptor.GetParticipantId())
        return;
    
    if (_hasCoordinatedSimulationStop)
    {
        if (command.kind == ParticipantCommand::Kind::Restart)
        {
            Restart(std::string{"Received ParticipantCommand::"} + to_string(command.kind));
        }
        else if (command.kind == ParticipantCommand::Kind::Shutdown)
        {
            Shutdown("Received ParticipantCommand::Shutdown");
        }
    }
}

auto LifecycleService::GetTimeSyncService() const -> ITimeSyncService*
{
    return _timeSyncService;
}



void LifecycleService::ReceiveMsg(const IServiceEndpoint* from, const SystemCommand& command)
{
    // Ignore messages if the lifecycle is not being executed yet
    if (!_isRunning)
    {
        // TODO this should be handled as a late joining scenario instead of an error...
        std::stringstream msg;
        msg << "Received SystemCommand::" << command.kind
            << " before LifecycleService::StartLifecycle(...) was called."
            << " Origin of current command was " << from->GetServiceDescriptor();
        ReportError(msg.str());
        return;
    }

    switch (command.kind)
    {
    case SystemCommand::Kind::Invalid: break;

    case SystemCommand::Kind::Run:
        if (!_hasCoordinatedSimulationStart)
        {
            _logger->Info(
                "Received SystemCommand::Start, but ignored it because coordinatedSimulationStart was not set.");
            return;
        }
        else
        {
            _lifecycleManagement.Run("Received SystemCommand::Run");
            return;
        }
        break;

    case SystemCommand::Kind::Stop:

        if (!_hasCoordinatedSimulationStop)
        {
            _logger->Info(
                "Received SystemCommand::Stop, but ignored it because coordinatedSimulationStop was not set.");
            return;
        }
        Stop("Received SystemCommand::Stop");
        return;

    case SystemCommand::Kind::AbortSimulation:
        AbortSimulation("Received SystemCommand::AbortSimulation");
        return;
    default: break;
    }

    // We should not reach this point in normal operation.
    ReportError("Received SystemCommand::" + to_string(command.kind)
                + " while in ParticipantState::" + to_string(State()));
}

void LifecycleService::ChangeState(ParticipantState newState, std::string reason)
{
    _status.state = newState;
    _status.enterReason = std::move(reason);
    _status.enterTime = std::chrono::system_clock::now();
    _status.refreshTime = _status.enterTime;

    std::stringstream ss;
    ss << "New ParticipantState: " << newState << "; reason: " << _status.enterReason;
    _logger->Info(ss.str());

    SendMsg(_status);
}

void LifecycleService::SetTimeSyncService(TimeSyncService* timeSyncService)
{
    _timeSyncService = timeSyncService;
}

void LifecycleService::NewSystemState(SystemState systemState)
{
    _lifecycleManagement.NewSystemState(systemState);
}

void LifecycleService::SetTimeSyncActive(bool isTimeSyncAcvice)
{
    _timeSyncActive = isTimeSyncAcvice;
}

bool LifecycleService::IsTimeSyncActive() const
{
    return _timeSyncActive;
}

} // namespace Orchestration
} // namespace Services
} // namespace SilKit
