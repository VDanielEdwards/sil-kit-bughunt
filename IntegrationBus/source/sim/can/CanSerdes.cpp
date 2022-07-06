// Copyright (c) Vector Informatik GmbH. All rights reserved.

#include "MessageBuffer.hpp"
#include "silkit/services/can/CanDatatypes.hpp"
#include "CanSerdes.hpp"

namespace SilKit {
namespace Services {
namespace Can {

SilKit::Core::MessageBuffer& operator<<(SilKit::Core::MessageBuffer& buffer, const CanFrameEvent& msg)
{
    buffer << msg.transmitId
        << msg.timestamp
        << msg.frame.canId
        << *reinterpret_cast<const uint8_t*>(&msg.frame.flags)
        << msg.frame.dlc
        << msg.frame.dataField
        << msg.direction
        << msg.userContext
        ;
    return buffer;
}

SilKit::Core::MessageBuffer& operator>>(SilKit::Core::MessageBuffer& buffer, CanFrameEvent& msg)
{
    uint8_t flags;
    uint8_t dlc;
    buffer >> msg.transmitId
        >> msg.timestamp
        >> msg.frame.canId
        >> flags
        >> dlc
        >> msg.frame.dataField
        >> msg.direction
        >> msg.userContext
        ;
    *reinterpret_cast<uint8_t*>(&msg.frame.flags) = flags;
    msg.frame.dlc = dlc;
    return buffer;
}

SilKit::Core::MessageBuffer& operator<<(SilKit::Core::MessageBuffer& buffer, const CanFrameTransmitEvent& ack)
{
    buffer << ack.transmitId
           << ack.canId
           << ack.timestamp
           << ack.status
           << ack.userContext;
    return buffer;
}

SilKit::Core::MessageBuffer& operator>>(SilKit::Core::MessageBuffer& buffer, CanFrameTransmitEvent& ack)
{
    buffer >> ack.transmitId
           >> ack.canId
           >> ack.timestamp
           >> ack.status
           >> ack.userContext;
    return buffer;
}

SilKit::Core::MessageBuffer& operator<<(SilKit::Core::MessageBuffer& buffer, const CanControllerStatus& msg)
{
    buffer << msg.timestamp
           << msg.controllerState
           << msg.errorState;
    return buffer;
}

SilKit::Core::MessageBuffer& operator>>(SilKit::Core::MessageBuffer& buffer, CanControllerStatus& msg)
{
    buffer >> msg.timestamp
           >> msg.controllerState
           >> msg.errorState;
    return buffer;
}

SilKit::Core::MessageBuffer& operator<<(SilKit::Core::MessageBuffer& buffer, const CanConfigureBaudrate& msg)
{
    buffer << msg.baudRate
           << msg.fdBaudRate;
    return buffer;
}

SilKit::Core::MessageBuffer& operator>>(SilKit::Core::MessageBuffer& buffer, CanConfigureBaudrate& msg)
{
    buffer >> msg.baudRate
           >> msg.fdBaudRate;
    return buffer;
}

SilKit::Core::MessageBuffer& operator<<(SilKit::Core::MessageBuffer& buffer, const CanSetControllerMode& msg)
{
    buffer << *reinterpret_cast<const uint8_t*>(&msg.flags)
           << msg.mode;
    return buffer;
}

SilKit::Core::MessageBuffer& operator>>(SilKit::Core::MessageBuffer& buffer, CanSetControllerMode& msg)
{
    uint8_t flags;
    buffer >> flags
           >> msg.mode;
    *reinterpret_cast<uint8_t*>(&msg.flags) = flags;
    return buffer;
}

using namespace SilKit::Core;
// when changing any of the datatypes, add transparent compatibility code here,
// based on the buffer.GetProtocolVersion()

//////////////////////////////////////////////////////////////////////
// Serialize
//////////////////////////////////////////////////////////////////////
void Serialize(MessageBuffer& buffer, const Services::Can::CanFrameEvent& msg)
{
    buffer << msg;
    return;
}

void Serialize(MessageBuffer& buffer, const Services::Can::CanFrameTransmitEvent& msg)
{
    buffer << msg;
    return;
}

void Serialize(MessageBuffer& buffer, const Services::Can::CanControllerStatus& msg)
{
    buffer << msg;
    return;
}

void Serialize(MessageBuffer& buffer, const Services::Can::CanConfigureBaudrate& msg)
{
    buffer << msg;
    return;
}

void Serialize(MessageBuffer& buffer, const Services::Can::CanSetControllerMode& msg)
{
    buffer << msg;
    return;
}

//////////////////////////////////////////////////////////////////////
// Deserialize
//////////////////////////////////////////////////////////////////////
void Deserialize(MessageBuffer& buffer, Services::Can::CanFrameEvent& out)
{
   buffer >> out;
}

void Deserialize(MessageBuffer& buffer, Services::Can::CanFrameTransmitEvent& out)
{
   buffer >> out;
}

void Deserialize(MessageBuffer& buffer, Services::Can::CanControllerStatus& out)
{
   buffer >> out;
}

void Deserialize(MessageBuffer& buffer, Services::Can::CanConfigureBaudrate& out)
{
   buffer >> out;
}

void Deserialize(MessageBuffer& buffer, Services::Can::CanSetControllerMode& out)
{
   buffer >> out;
}

} // namespace Can    
} // namespace Services
} // namespace SilKit
