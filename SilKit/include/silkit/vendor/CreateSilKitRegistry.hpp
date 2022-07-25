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

#include <memory>
#include <functional>

#include "silkit/SilKitMacros.hpp"
#include "silkit/vendor/ISilKitRegistry.hpp"
#include "silkit/config/IParticipantConfiguration.hpp"


namespace SilKit {
namespace Vendor {
namespace Vector {

/*! \brief Create an instance of ISilKitRegistry.
*
* This API is specific to the Vector Informatik implementation of the SIL Kit.
* It is required as a central service for other SIL Kit participants to register with.
* Throws std::runtime_error on error.
*/

SilKitAPI auto CreateSilKitRegistry(std::shared_ptr<Config::IParticipantConfiguration> config)
    -> std::unique_ptr<ISilKitRegistry>;

}// namespace Vector
}// namespace Vendor
}// namespace SilKit
