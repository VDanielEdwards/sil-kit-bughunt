// Copyright (c) Vector Informatik GmbH. All rights reserved.

#pragma once

#include "ib/mw/sync/SyncDatatypes.hpp"
#include "IIbEndpoint.hpp"
#include "IIbSender.hpp"

namespace ib {
namespace mw {
namespace sync {

class IIbToSystemMonitor
    : public mw::IIbEndpoint<ParticipantStatus, ExpectedParticipants>
    , public mw::IIbSender<ExpectedParticipants>
{
};

} // namespace sync
} // namespace mw
} // namespace ib
