// Copyright (c) Vector Informatik GmbH. All rights reserved.

#include "DataPublisher.hpp"
#include "IParticipantInternal.hpp"
#include "DataMessageDatatypeUtils.hpp"

namespace ib {
namespace sim {
namespace data {

DataPublisher::DataPublisher(mw::IParticipantInternal* participant, mw::sync::ITimeProvider* timeProvider,
                             const std::string& topic, const std::string& mediaType,
                             const std::map<std::string, std::string>& labels, const std::string& pubUUID)
    : _topic{topic}
    , _mediaType{mediaType}
    , _labels{labels}
    , _pubUUID{pubUUID}
    , _timeProvider{timeProvider}
    , _participant{participant}
{
}

void DataPublisher::Publish(std::vector<uint8_t> data)
{
    DataMessage msg{std::move(data)};
    _participant->SendIbMessage(this, std::move(msg));
}

void DataPublisher::Publish(const uint8_t* data, std::size_t size)
{
    Publish({data, data + size});
}

void DataPublisher::SetTimeProvider(mw::sync::ITimeProvider* provider)
{
    _timeProvider = provider;
}

} // namespace data
} // namespace sim
} // namespace ib
