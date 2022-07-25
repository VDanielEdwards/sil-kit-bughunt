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

#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <string>

#include "VAsioPeerInfo.hpp"
#include "ProtocolVersion.hpp" // for current ProtocolVersion in RegistryMsgHeader

namespace SilKit {
namespace Core {

struct RegistryMsgHeader
{
    std::array<uint8_t, 4> preambel{{'V', 'I', 'B', '-'}};
    // If versionHigh/Low changes here, update SILKIT version range .
    // Also, ensure backwards compatibility in the Ser/Des code path.
    // See VAsioProtcolVersion.hpp
    uint16_t versionHigh;
    uint16_t versionLow;
    RegistryMsgHeader()
        : versionHigh{CurrentProtocolVersion().major}
        , versionLow{CurrentProtocolVersion().minor}
    {
    }
};

struct VAsioMsgSubscriber
{
    EndpointId receiverIdx;
    std::string networkName;
    std::string msgTypeName;
    uint32_t version{0};
};

struct SubscriptionAcknowledge
{
    enum class Status : uint8_t {
        Failed = 0,
        Success = 1
    };
    Status status;
    VAsioMsgSubscriber subscriber;
};

struct ParticipantAnnouncement
{
    RegistryMsgHeader messageHeader;
    SilKit::Core::VAsioPeerInfo peerInfo;
};

struct ParticipantAnnouncementReply
{
    RegistryMsgHeader remoteHeader;
    enum class Status : uint8_t {
        Failed = 0,
        Success = 1
    };
    Status status{Status::Failed}; //default for failure to deserialize
    std::vector<VAsioMsgSubscriber> subscribers;
};

struct KnownParticipants
{
    RegistryMsgHeader messageHeader;
    std::vector<SilKit::Core::VAsioPeerInfo> peerInfos;
};

enum class RegistryMessageKind : uint8_t
{
    Invalid = 0,
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // !! DO NOT CHANGE THE VALUE OF ParticipantAnnouncement !!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // The ParticipantAnnouncement is the first message transmitted over a new
    // connection and carries the protocol version. Thus, changing the enum
    // value of ParticipantAnnouncement will break protocol break detections
    // with older participants.
    ParticipantAnnouncement = 1,
    ParticipantAnnouncementReply = 2,
    KnownParticipants = 3
};

// ================================================================================
//  Inline Implementations
// ================================================================================
inline bool operator!=(const RegistryMsgHeader& lhs, const RegistryMsgHeader& rhs)
{
    return lhs.preambel != rhs.preambel
        || lhs.versionHigh != rhs.versionHigh
        || lhs.versionLow != rhs.versionLow;
}

inline bool operator==(const RegistryMsgHeader& lhs, const RegistryMsgHeader& rhs)
{
    return lhs.preambel == rhs.preambel
        && lhs.versionHigh == rhs.versionHigh
        && lhs.versionLow == rhs.versionLow
        ;
}

inline bool operator==(const VAsioMsgSubscriber& lhs, const VAsioMsgSubscriber& rhs)
{
    return lhs.receiverIdx == rhs.receiverIdx 
        && lhs.networkName == rhs.networkName
        && lhs.msgTypeName == rhs.msgTypeName;
}

} // namespace Core
} // namespace SilKit
