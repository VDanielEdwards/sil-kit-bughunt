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

#include <unordered_map>

#include "silkit/services/pubsub/DataMessageDatatypes.hpp"
#include "silkit/util/HandlerId.hpp"

#include "Hash.hpp"
#include "WireDataMessages.hpp"

namespace SilKit {
namespace Services {
namespace PubSub {

class DataSubscriberInternal;

bool operator==(const DataMessageEvent& lhs, const DataMessageEvent& rhs);

bool operator==(const WireDataMessageEvent& lhs, const WireDataMessageEvent& rhs);

bool MatchMediaType(const std::string& subMediaType, const std::string& pubMediaType);

bool MatchLabels(const std::map<std::string, std::string>& innerSet, const std::map<std::string, std::string>& outerSet);

struct ExplicitDataMessageHandlerInfo
{
    HandlerId id;
    std::string mediaType;
    std::map<std::string, std::string> labels;
    DataMessageHandlerT explicitDataMessageHandler;
    std::unordered_map<DataSubscriberInternal*, HandlerId> registeredInternalSubscribers;
};


struct SourceInfo
{
    std::string mediaType;
    std::map<std::string, std::string> labels;

    bool operator==(const SourceInfo& other) const
    {
        return other.mediaType == mediaType && std::equal(other.labels.begin(), other.labels.end(), labels.begin());
    }
    struct HashFunction
    {
        uint64_t operator()(const SourceInfo& s) const { 
            auto hMediaType = SilKit::Util::Hash::Hash(s.mediaType);
            auto hLabels = SilKit::Util::Hash::Hash(s.labels);
            return SilKit::Util::Hash::HashCombine(hMediaType, hLabels);
        }
    };
};

} // namespace PubSub
} // namespace Services
} // namespace SilKit
