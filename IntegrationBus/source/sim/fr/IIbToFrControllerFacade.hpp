// Copyright (c) Vector Informatik GmbH. All rights reserved.

#pragma once

#include "IIbEndpoint.hpp"
#include "IIbSender.hpp"
#include "ib/sim/fr/fwd_decl.hpp"

namespace ib {
namespace sim {
namespace fr {

/*! \brief IIbToFrControllerFacade interface
 *
 *  Used by the ComAdapter, implemented by the FrControllerFacade
 */
class IIbToFrControllerFacade
    : public ib::mw::IIbEndpoint<FrMessage, FrMessageAck, FrSymbol, FrSymbolAck, CycleStart, PocStatus>
    , public ib::mw::IIbSender</*Fr*/ FrMessage, FrMessageAck, FrSymbol, FrSymbolAck, 
                                /*FrProxy*/ HostCommand, ControllerConfig, TxBufferConfigUpdate, TxBufferUpdate>
{
};

} // namespace fr
} // namespace sim
} // namespace ib
