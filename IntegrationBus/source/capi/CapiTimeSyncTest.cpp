// Copyright (c) Vector Informatik GmbH. All rights reserved.
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "silkit/capi/SilKit.h"
#include "silkit/core/sync/all.hpp"

#include "MockParticipant.hpp"

namespace {
using namespace SilKit::Core::Orchestration;
using SilKit::Core::Tests::DummyParticipant;

class CapiTimeSyncTest : public testing::Test
{
protected: 
    SilKit::Core::Tests::DummyParticipant mockParticipant;

    CapiTimeSyncTest() {}

};

void SimTask(void* /*context*/, SilKit_Participant* /*participant*/, SilKit_NanosecondsTime /*now*/) {}

TEST_F(CapiTimeSyncTest, participant_state_handling_nullpointer_params)
{
    SilKit_ReturnCode returnCode;
    returnCode = SilKit_Participant_SetPeriod(nullptr, 1000);
    EXPECT_EQ(returnCode, SilKit_ReturnCode_BADPARAMETER);

    returnCode = SilKit_Participant_SetSimulationTask(nullptr, nullptr, &SimTask);
    EXPECT_EQ(returnCode, SilKit_ReturnCode_BADPARAMETER);
    returnCode = SilKit_Participant_SetSimulationTask((SilKit_Participant*)&mockParticipant, nullptr, nullptr);
    EXPECT_EQ(returnCode, SilKit_ReturnCode_BADPARAMETER);
}

TEST_F(CapiTimeSyncTest, participant_state_handling_function_mapping)
{
    SilKit_ReturnCode returnCode;
    EXPECT_CALL(mockParticipant.mockTimeSyncService,
        SetPeriod(testing::_)
    ).Times(testing::Exactly(1));
    returnCode = SilKit_Participant_SetPeriod((SilKit_Participant*)&mockParticipant, 1000);

    EXPECT_CALL(mockParticipant.mockTimeSyncService,
        SetSimulationTask(testing::Matcher<SilKit::Core::Orchestration::ITimeSyncService::SimTaskT>(testing::_))
    ).Times(testing::Exactly(1));
    returnCode = SilKit_Participant_SetSimulationTask((SilKit_Participant*)&mockParticipant, nullptr, &SimTask);
    EXPECT_EQ(returnCode, SilKit_ReturnCode_SUCCESS);
}

} //namespace