// Copyright (c) Vector Informatik GmbH. All rights reserved.

#include <chrono>
#include <cstdlib>
#include <thread>
#include <future>

#include "CreateParticipant.hpp"
#include "VAsioRegistry.hpp"

#include "silkit/core/sync/all.hpp"
#include "silkit/util/functional.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "GetTestPid.hpp"
#include "ConfigurationTestUtils.hpp"

namespace {

using namespace std::chrono_literals;
using namespace SilKit::Core;
using namespace SilKit::Core::Orchestration;

using testing::_;
using testing::A;
using testing::An;
using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;

class VAsioNetworkITest : public testing::Test
{
protected:
    struct Callbacks
    {
        MOCK_METHOD0(CommunicationReadyHandler, void());
        MOCK_METHOD0(StopHandler, void());
        MOCK_METHOD0(ShutdownHandler, void());
        MOCK_METHOD1(ParticipantStateHandler, void(ParticipantState)); // Helper to only check for status.state; no longer part of the API
    };

protected:
    VAsioNetworkITest() = default;

    auto SetTargetState(ParticipantState state)
    {
        _targetState = state;
        _targetStatePromise = std::promise<void>{};
        auto future = _targetStatePromise.get_future();
        if (_currentState == state) 
        {
            // If we are already in the correct state, we have to set the promise immediately.
            // This happens if the ParticipantStateHandler is triggered before setting up the 
            // expectation here.
            _targetStatePromise.set_value();
        }
        return future;
    }

    void ParticipantStateHandler(const ParticipantState& state)
    {
        callbacks.ParticipantStateHandler(state);
        _currentState = state;

        if (_currentState == _targetState)
            _targetStatePromise.set_value();
    }

protected:
    ParticipantState _targetState{ParticipantState::Invalid};
    std::promise<void> _targetStatePromise;
    ParticipantState _currentState;

    Callbacks callbacks;
};

TEST_F(VAsioNetworkITest, vasio_state_machine)
{
    auto registryUri = MakeTestRegistryUri();
    std::vector<std::string> syncParticipantNames{"TestUnit"};

    auto registry = std::make_unique<VAsioRegistry>(SilKit::Config::MakeEmptyParticipantConfiguration());
    registry->ProvideDomain(registryUri);

    // Setup Participant for TestController
    auto participant = CreateParticipantImpl(SilKit::Config::MakeEmptyParticipantConfiguration(), "TestController");

    participant->JoinSilKitDomain(registryUri);
    auto systemController = participant->GetSystemController();
    systemController->SetWorkflowConfiguration({syncParticipantNames});
    auto monitor = participant->GetSystemMonitor();
    monitor->AddParticipantStatusHandler([this](ParticipantStatus status) {
        this->ParticipantStateHandler(status.state);
    });

    // Setup Participant for Test Unit
    auto participantTestUnit = CreateParticipantImpl(SilKit::Config::MakeEmptyParticipantConfiguration(), "TestUnit");
    participantTestUnit->JoinSilKitDomain(registryUri);
    auto* lifecycleService = participantTestUnit->GetLifecycleService();
    auto* timeSyncService = lifecycleService->GetTimeSyncService();

    lifecycleService->SetCommunicationReadyHandler([&callbacks = callbacks]() {
        callbacks.CommunicationReadyHandler();
    });
    timeSyncService->SetSimulationTask([](auto /*now*/, auto /*duration*/) {
    });

    lifecycleService->SetStopHandler([&callbacks = callbacks]() {
        callbacks.StopHandler();
    });
    lifecycleService->SetShutdownHandler([&callbacks = callbacks]() {
        callbacks.ShutdownHandler();
    });

    std::string participantName = "TestUnit";

    EXPECT_CALL(callbacks, CommunicationReadyHandler()).Times(1);
    EXPECT_CALL(callbacks, StopHandler()).Times(1);
    EXPECT_CALL(callbacks, ShutdownHandler()).Times(1);

    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::Invalid)).Times(0);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::ServicesCreated)).Times(1);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::CommunicationInitializing)).Times(1);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::CommunicationInitialized)).Times(1);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::ReadyToRun)).Times(1);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::Running)).Times(1);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::Stopping)).Times(1);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::Stopped)).Times(1);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::ShuttingDown)).Times(1);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::Shutdown)).Times(1);
    EXPECT_CALL(callbacks, ParticipantStateHandler(ParticipantState::Error)).Times(0);

    // Perform the actual test
    auto stateReached = SetTargetState(ParticipantState::ServicesCreated);
    auto finalState = lifecycleService->StartLifecycleWithSyncTime(timeSyncService, {true, true});
    EXPECT_EQ(stateReached.wait_for(5s), std::future_status::ready);

    stateReached = SetTargetState(ParticipantState::Running);
    systemController->Run();
    EXPECT_EQ(stateReached.wait_for(5s), std::future_status::ready);

    stateReached = SetTargetState(ParticipantState::Stopped);
    systemController->Stop();
    EXPECT_EQ(stateReached.wait_for(5s), std::future_status::ready);

    stateReached = SetTargetState(ParticipantState::Shutdown);
    systemController->Shutdown(participantName);
    EXPECT_EQ(stateReached.wait_for(5s), std::future_status::ready);

    ASSERT_EQ(finalState.wait_for(5s), std::future_status::ready);
    EXPECT_EQ(finalState.get(), ParticipantState::Shutdown);
}

} // anonymous namespace
