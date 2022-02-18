// Copyright (c) Vector Informatik GmbH. All rights reserved.

#pragma once

#include "MessageBuffer.hpp"

#include "SerdesMw.hpp"

#include "SyncDatatypes.hpp"

namespace ib {
namespace mw {
namespace sync {

inline MessageBuffer& operator<<(MessageBuffer& buffer, const NextSimTask& task)
{
    buffer << task.timePoint
           << task.duration;
    return buffer;
}
inline MessageBuffer& operator>>(MessageBuffer& buffer, NextSimTask& task)
{
    buffer >> task.timePoint
           >> task.duration;
    return buffer;
}

    
inline MessageBuffer& operator<<(MessageBuffer& buffer, const ParticipantCommand& cmd)
{
    buffer << cmd.participant
           << cmd.kind;
    return buffer;
}
inline MessageBuffer& operator>>(MessageBuffer& buffer, ParticipantCommand& cmd)
{
    buffer >> cmd.participant
           >> cmd.kind;
    return buffer;
}

    
inline MessageBuffer& operator<<(MessageBuffer& buffer, const SystemCommand& cmd)
{
    buffer << cmd.kind;
    return buffer;
}
inline MessageBuffer& operator>>(MessageBuffer& buffer, SystemCommand& cmd)
{
    buffer >> cmd.kind;
    return buffer;
}
    

inline MessageBuffer& operator<<(MessageBuffer& buffer, const ParticipantStatus& status)
{
    buffer << status.participantName
           << status.state
           << status.enterReason
           << status.enterTime
           << status.refreshTime;
    return buffer;
}
inline MessageBuffer& operator>>(MessageBuffer& buffer, ParticipantStatus& status)
{
    buffer >> status.participantName
           >> status.state
           >> status.enterReason
           >> status.enterTime
           >> status.refreshTime;
    return buffer;
}

inline MessageBuffer& operator<<(MessageBuffer& buffer, const ExpectedParticipants& participants)
{
    buffer << participants.names;
    return buffer;
}

inline MessageBuffer& operator>>(MessageBuffer& buffer, ExpectedParticipants& participants)
{
    buffer >> participants.names;
    return buffer;
}

} // namespace sync    
} // namespace mw
} // namespace ib
