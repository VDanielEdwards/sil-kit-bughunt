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

#include "DataSubscriberInternal.hpp"

#include "silkit/services/logging/ILogger.hpp"

namespace SilKit {
namespace Services {
namespace PubSub {

DataSubscriberInternal::DataSubscriberInternal(Core::IParticipantInternal* participant, Services::Orchestration::ITimeProvider* timeProvider,
                                               const std::string& topic, const std::string& mediaType,
                                               const std::map<std::string, std::string>& labels,
                                               DataMessageHandlerT defaultHandler, IDataSubscriber* parent)
    : _topic{topic}
    , _mediaType{mediaType}
    , _labels{labels}
    , _defaultHandler{std::move(defaultHandler)}
    , _parent{parent}
    , _timeProvider{timeProvider}
    , _participant{participant}
{
    (void)_participant;
}

void DataSubscriberInternal::SetDefaultDataMessageHandler(DataMessageHandlerT handler)
{
    _defaultHandler = std::move(handler);
}

auto DataSubscriberInternal::AddExplicitDataMessageHandler(DataMessageHandlerT handler) -> HandlerId
{
    return _explicitDataMessageHandlers.Add(std::move(handler));
}

void DataSubscriberInternal::RemoveExplicitDataMessageHandler(HandlerId handlerId)
{
    _explicitDataMessageHandlers.Remove(handlerId);
}

void DataSubscriberInternal::ReceiveMsg(const Core::IServiceEndpoint* from, const WireDataMessageEvent& dataMessageEvent)
{
    if (AllowMessageProcessing(from->GetServiceDescriptor(), _serviceDescriptor))
        return;

    ReceiveMessage(dataMessageEvent);
}

void DataSubscriberInternal::ReceiveMessage(const WireDataMessageEvent& dataMessageEvent)
{
    const auto anySpecificHandlerExecuted = _explicitDataMessageHandlers.InvokeAll(_parent, ToDataMessageEvent(dataMessageEvent));

    if (_defaultHandler && !anySpecificHandlerExecuted)
    {
        _defaultHandler(_parent, ToDataMessageEvent(dataMessageEvent));
    }

    if (!_defaultHandler && !anySpecificHandlerExecuted)
    {
        _participant->GetLogger()->Warn("DataSubscriber on topic " + _topic + " received data, but has no default or specific handler assigned");
    }
}

void DataSubscriberInternal::SetTimeProvider(Services::Orchestration::ITimeProvider* provider)
{
    _timeProvider = provider;
}


} // namespace PubSub
} // namespace Services
} // namespace SilKit
