// Copyright (c) Vector Informatik GmbH. All rights reserved.

#pragma once

#include "MessageBuffer.hpp"

#include "silkit/services/eth/EthernetDatatypes.hpp"

namespace SilKit {
namespace Services {
namespace Ethernet {

void Serialize(SilKit::Core::MessageBuffer& buffer, const EthernetFrameEvent& msg);
void Serialize(SilKit::Core::MessageBuffer& buffer, const EthernetFrameTransmitEvent& msg);
void Serialize(SilKit::Core::MessageBuffer& buffer, const EthernetStatus& msg);
void Serialize(SilKit::Core::MessageBuffer& buffer, const EthernetSetMode& msg);

void Deserialize(SilKit::Core::MessageBuffer& buffer, EthernetFrameEvent& out);
void Deserialize(SilKit::Core::MessageBuffer& buffer, EthernetFrameTransmitEvent& out);
void Deserialize(SilKit::Core::MessageBuffer& buffer, EthernetStatus& out);
void Deserialize(SilKit::Core::MessageBuffer& buffer, EthernetSetMode& out);


} // namespace Ethernet    
} // namespace Services
} // namespace SilKit
