// Copyright (c) 2022 Vector Informatik GmbH
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once


#include "IConnectionMethods.hpp"

#include "gtest/gtest.h"
#include "gmock/gmock.h"


namespace SilKit {
namespace Core {


struct MockConnectionMethods : IConnectionMethods
{
    MOCK_METHOD(std::unique_ptr<IConnectPeer>, MakeConnectPeer, (VAsioPeerInfo const &), (override));
    MOCK_METHOD(std::unique_ptr<SilKit::Core::IVAsioPeer>, MakeVAsioPeer, (std::unique_ptr<IRawByteStream>),
                (override));

    MOCK_METHOD(void, HandleConnectedPeer, (SilKit::Core::IVAsioPeer *), (override));
    MOCK_METHOD(void, AddPeer, (std::unique_ptr<SilKit::Core::IVAsioPeer>), (override));

    MOCK_METHOD(bool, TryRemoteConnectRequest, (VAsioPeerInfo const &), (override));
    MOCK_METHOD(bool, TryProxyConnect, (VAsioPeerInfo const &), (override));
};


} // namespace Core
} // namespace SilKit
