// Copyright (c) Vector Informatik GmbH. All rights reserved.

#pragma once

#include <vector>
#include <future>

#include "ib/mw/fwd_decl.hpp"
#include "ib/sim/data/IDataPublisher.hpp"
#include "ib/mw/sync/ITimeConsumer.hpp"

#include "IIbToDataPublisher.hpp"
#include "IComAdapterInternal.hpp"

namespace ib {
namespace sim {
namespace data {

class DataPublisher
    : public IDataPublisher
    , public IIbToDataPublisher
    , public mw::sync::ITimeConsumer
    , public mw::IIbServiceEndpoint
{
public:
    DataPublisher(mw::IComAdapterInternal* comAdapter, mw::sync::ITimeProvider* timeProvider, const std::string& topic,
                  DataExchangeFormat dataExchangeFormat, const std::map<std::string, std::string>& labels,
                  const std::string& pubUUID, size_t history);
    
    void Publish(std::vector<uint8_t> data) override;
    void Publish(const uint8_t* data, std::size_t size) override;

    //ib::mw::sync::ITimeConsumer
    void SetTimeProvider(mw::sync::ITimeProvider* provider) override;

    // IIbServiceEndpoint
    inline void SetServiceDescriptor(const mw::ServiceDescriptor& serviceDescriptor) override;
    inline auto GetServiceDescriptor() const -> const mw::ServiceDescriptor & override;


private:
    std::string _topic;
    DataExchangeFormat _dataExchangeFormat;
    std::map<std::string, std::string> _labels;
    std::string _pubUUID;
    size_t _history;

    mw::IComAdapterInternal* _comAdapter{nullptr};
    mw::sync::ITimeProvider* _timeProvider{nullptr};
    mw::ServiceDescriptor _serviceDescriptor{};
};

// ================================================================================
//  Inline Implementations
// ================================================================================

void DataPublisher::SetServiceDescriptor(const mw::ServiceDescriptor& serviceDescriptor)
{
    _serviceDescriptor = serviceDescriptor;
}

auto DataPublisher::GetServiceDescriptor() const -> const mw::ServiceDescriptor&
{
    return _serviceDescriptor;
}

} // namespace data
} // namespace sim
} // namespace ib
