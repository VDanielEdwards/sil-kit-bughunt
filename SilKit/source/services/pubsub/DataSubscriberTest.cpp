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

#include "DataSubscriber.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "functional.hpp"

#include "MockParticipant.hpp"

#include "DataPublisher.hpp"
#include "DataMessageDatatypeUtils.hpp"

#include "YamlParser.hpp"

namespace {

using namespace std::chrono_literals;

using namespace testing;

using namespace SilKit;
using namespace SilKit::Core;
using namespace SilKit::Services::PubSub;

using ::SilKit::Core::Tests::DummyParticipant;

class MockParticipant : public DummyParticipant
{
public:
    MOCK_METHOD(Services::PubSub::DataSubscriberInternal*, CreateDataSubscriberInternal,
                (const std::string& /*topic*/, const std::string& /*linkName*/, const std::string& /*mediaType*/,
                 (const std::vector<SilKit::Services::Label>& ) /*publisherLabels*/,
                 Services::PubSub::DataMessageHandlerT /*callback*/, Services::PubSub::IDataSubscriber* /*parent*/),
                (override));
};

class DataSubscriberTest : public ::testing::Test
{
protected:
    struct Callbacks
    {
        MOCK_METHOD(void, ReceiveDataDefault, (IDataSubscriber*, const DataMessageEvent& dataMessageEvent));
        MOCK_METHOD(void, ReceiveDataExplicitA, (IDataSubscriber*, const DataMessageEvent& dataMessageEvent));
        MOCK_METHOD(void, ReceiveDataExplicitB, (IDataSubscriber*, const DataMessageEvent& dataMessageEvent));

        MOCK_METHOD(void, NewDataPublisher, (IDataSubscriber*, const NewDataPublisherEvent& newDataPublisherEvent));
    };

protected:
    DataSubscriberTest()
        : subscriber{&participant,
                     participant.GetTimeProvider(), matchingDataSpec,
                     SilKit::Util::bind_method(&callbacks, &Callbacks::ReceiveDataDefault)}
        , publisher{&participant, participant.GetTimeProvider(), dataSpec, publisherUuid}
    {
        subscriber.SetServiceDescriptor(from_endpointAddress(subscriberEndpointAddress));
        SetupPublisherServiceDescriptor(publisher, publisherUuid, publisherEndpointAddress);
    }

private:
    void SetupPublisherServiceDescriptor(DataPublisher& dataPublisher, const std::string& uuid,
                                         const EndpointAddress& endpointAddress)
    {
        auto publisherServiceDescriptor = from_endpointAddress(endpointAddress);
        publisherServiceDescriptor.SetSupplementalDataItem(Core::Discovery::supplKeyDataPublisherTopic, topic);
        publisherServiceDescriptor.SetSupplementalDataItem(Core::Discovery::supplKeyDataPublisherMediaType, mediaType);
        publisherServiceDescriptor.SetSupplementalDataItem(Core::Discovery::supplKeyDataPublisherPubLabels,
                                                           labelsSerialized);
        publisherServiceDescriptor.SetSupplementalDataItem(Core::Discovery::supplKeyDataPublisherPubUUID, uuid);
        dataPublisher.SetServiceDescriptor(publisherServiceDescriptor);
    }

protected:
    ServiceDescriptor MakePublisherServiceDescriptor(const std::string& uuid, const EndpointAddress& endpointAddress)
    {
        auto publisherServiceDescriptor = from_endpointAddress(endpointAddress);
        publisherServiceDescriptor.SetSupplementalDataItem(Core::Discovery::supplKeyDataPublisherTopic, topic);
        publisherServiceDescriptor.SetSupplementalDataItem(Core::Discovery::supplKeyDataPublisherMediaType, mediaType);
        publisherServiceDescriptor.SetSupplementalDataItem(Core::Discovery::supplKeyDataPublisherPubLabels,
                                                           labelsSerialized);
        publisherServiceDescriptor.SetSupplementalDataItem(Core::Discovery::supplKeyDataPublisherPubUUID, uuid);
        return publisherServiceDescriptor;
    }

protected:
    const std::vector<uint8_t> sampleData{0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};

    const EndpointAddress subscriberEndpointAddress{4, 5};

    const EndpointAddress publisherEndpointAddress{6, 7};
    const std::string publisherUuid{"pubUUID"};

    const EndpointAddress publisher2EndpointAddress{8, 9};
    const std::string publisher2Uuid{"pubUUID-2"};

    const std::string topic{"Topic"};
    const std::string mediaType{};
    const std::vector<SilKit::Services::Label> labels;
    SilKit::Services::PubSub::DataSubscriberSpec matchingDataSpec{topic, mediaType};
    SilKit::Services::PubSub::DataPublisherSpec dataSpec{topic, mediaType};
    const std::string labelsSerialized{SilKit::Config::Serialize<std::vector<SilKit::Services::Label>>(labels)};
    Callbacks callbacks;
    MockParticipant participant;
    DataSubscriber subscriber;
    DataPublisher publisher;
};

struct CreateSubscriberInternalMock
{
    MockParticipant* participant;
    std::unique_ptr<DataSubscriberInternal> dataSubscriberInternal;

    auto operator()(const std::string& topic, const std::string& /*linkName*/, const std::string& mediaType,
                    const std::vector<SilKit::Services::Label>& labels, Services::PubSub::DataMessageHandlerT defaultHandler,
                    Services::PubSub::IDataSubscriber* parent) -> DataSubscriberInternal*
    {
        dataSubscriberInternal = std::make_unique<DataSubscriberInternal>(
            participant, participant->GetTimeProvider(), topic, mediaType, labels, std::move(defaultHandler), parent);
        return dataSubscriberInternal.get();
    }
};

} // anonymous namespace
