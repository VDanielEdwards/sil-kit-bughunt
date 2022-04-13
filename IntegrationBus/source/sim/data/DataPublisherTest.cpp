// Copyright (c) Vector Informatik GmbH. All rights reserved.

#include "DataPublisher.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "ib/util/functional.hpp"

#include "MockParticipant.hpp"

#include "DataMessageDatatypeUtils.hpp"

namespace {

using namespace std::chrono_literals;

using namespace testing;

using namespace ib;
using namespace ib::mw;
using namespace ib::sim::data;

using ::ib::mw::test::DummyParticipant;

class MockParticipant : public DummyParticipant
{
public:

    MOCK_METHOD(void, SendIbMessage, (const IIbServiceEndpoint*, DataMessage&&));
};

class DataPublisherTest : public ::testing::Test
{
protected:
    DataPublisherTest()
        : publisher{ &participant, participant.GetTimeProvider(), "Topic", {}, {}, "pubUUID" }
    {
        publisher.SetServiceDescriptor(from_endpointAddress(portAddress));
    }

protected:
    const EndpointAddress portAddress{4, 5};
    const std::vector<uint8_t> sampleData{0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};

    MockParticipant participant;
    DataPublisher publisher;
};

TEST_F(DataPublisherTest, publish_vector)
{
    DataMessage msg{sampleData};

    EXPECT_CALL(participant, SendIbMessage(&publisher, std::move(msg)))
        .Times(1);

    publisher.Publish(sampleData);
}

TEST_F(DataPublisherTest, publish_raw)
{
    DataMessage msg{sampleData};

    EXPECT_CALL(participant, SendIbMessage(&publisher, std::move(msg)))
        .Times(1);

    publisher.Publish(sampleData.data(), sampleData.size());
}

} // anonymous namespace
