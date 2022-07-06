// Copyright (c) 2020 Vector Informatik GmbH. All rights reserved.
#pragma once

#include <cstdint>
#include <functional>

#include "silkit/core/logging/fwd_decl.hpp"

namespace SilKit {
namespace Vendor {
inline namespace Vector {

//! \brief Dedicated SilKit registry for the VAsio middleware.
//         This is a loadable runtime extension that is non-redistributable.
class ISilKitRegistry
{
public:
    virtual ~ISilKitRegistry() = default;
    //! \brief Register the handler that is called when all participants are connected
    virtual void SetAllConnectedHandler(std::function<void()> handler) = 0;
    //! \brief Register the handler that is called when all participants are disconnected
    virtual void SetAllDisconnectedHandler(std::function<void()> handler) = 0;
    //! \brief Returns the logger that is used by the SilKit registry.
    virtual auto GetLogger() -> Core::Logging::ILogger* = 0;
    //! \brief Provide the VAsio domain with the given listening URI with scheme silkit://. e.g. silkit://localhost:8500
    virtual void ProvideDomain(const std::string& listenUri) = 0;
};


}//end namespace Vector
}//end namespace Vendor
}//end namespace SilKit
