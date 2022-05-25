// Copyright (c) Vector Informatik GmbH. All rights reserved.
#pragma once
#include "VAsioMsgKind.hpp"
#include "VAsioDatatypes.hpp"
#include "SerializedMessageTraits.hpp"
#include "MessageBuffer.hpp"

// Component specific Serialize/Deserialize functions
#include "VAsioSerdes.hpp"
#include "CanSerdes.hpp"
#include "LinSerdes.hpp"
#include "EthernetSerdes.hpp"
#include "FlexraySerdes.hpp"
#include "RpcSerdes.hpp"
#include "InternalSerdes.hpp"
#include "SyncSerdes.hpp"
#include "ServiceSerdes.hpp"
#include "LoggingSerdes.hpp"
#include "DataSerdes.hpp"

namespace ib {
namespace mw {

//Helper to allow calling Deserialize(MessageBuffer&, T&) inside of template method SerializedMessage::Deserialize<T>
template<typename... Args>
auto AdlDeserialize(Args&&... args) -> decltype(auto)
{
	return Deserialize(std::forward<Args>(args)...);
}

// A serialized message used as binary wire format for the VAsio transport.
class SerializedMessage
{
public: //defaulted CTors
	SerializedMessage(SerializedMessage&&) = default;
	SerializedMessage& operator=(SerializedMessage&&) = default;
	SerializedMessage(const SerializedMessage&) = delete;
	SerializedMessage& operator=(const SerializedMessage&) = delete;

public: // Sending a SerializedMessage: from T to binary blob
	template<typename MessageT>
    explicit SerializedMessage(const MessageT& message);
	// Sim messages have additional parameters:
	template<typename MessageT>
	explicit SerializedMessage(const MessageT& message , EndpointAddress endpointAddress, EndpointId remoteIndex);
	template<typename MessageT>
	explicit SerializedMessage(ProtocolVersion version, const MessageT& message);

	auto ReleaseStorage() -> std::vector<uint8_t>;

public: // Receiving a SerializedMessage: from binary blob to IbMessage<T>
	explicit SerializedMessage(std::vector<uint8_t>&& blob);

	template<typename ApiMessageT>
	auto Deserialize() -> ApiMessageT;

	auto GetMessageKind() const -> VAsioMsgKind;
	auto GetRegistryKind() const -> RegistryMessageKind;
	auto GetRemoteIndex() const -> EndpointId;
	auto GetEndpointAddress() const -> EndpointAddress;
	void SetProtocolVersion(ProtocolVersion version);
	auto PeekRegistryMessageHeader() const -> RegistryMsgHeader;

private:
	void WriteNetworkHeaders();
	void ReadNetworkHeaders();
	// network headers, some members are optional depending on messageKind
	uint32_t _messageSize{0};
	VAsioMsgKind _messageKind{VAsioMsgKind::Invalid};
	RegistryMessageKind _registryKind{RegistryMessageKind::Invalid};
	// For simMsg
	EndpointAddress _endpointAddress{};
	EndpointId _remoteIndex{0};

	MessageBuffer _buffer;
};

//////////////////////////////////////////////////////////////////////
// Inline Implementations
//////////////////////////////////////////////////////////////////////
template <typename MessageT>
SerializedMessage::SerializedMessage(const MessageT& message)
{
    _messageKind = messageKind<MessageT>();
    _registryKind = registryMessageKind<MessageT>();
    WriteNetworkHeaders();
    Serialize(_buffer, message);
}

template <typename MessageT>
SerializedMessage::SerializedMessage(ProtocolVersion version, const MessageT& message)
{
    _messageKind = messageKind<MessageT>();
    _registryKind = registryMessageKind<MessageT>();
    _buffer.SetFormatVersion(version);
    WriteNetworkHeaders();
    Serialize(_buffer, message);
}

template <typename MessageT>
SerializedMessage::SerializedMessage(const MessageT& message, EndpointAddress endpointAddress, EndpointId remoteIndex)
{
    _remoteIndex = remoteIndex;
    _endpointAddress = endpointAddress;
    _messageKind = messageKind<MessageT>();
    _registryKind = registryMessageKind<MessageT>();
    WriteNetworkHeaders();
    Serialize(_buffer, message);
}

template <typename ApiMessageT>
auto SerializedMessage::Deserialize() -> ApiMessageT
{
    ApiMessageT value{};
    AdlDeserialize(_buffer, value);
    return value;
}

} //mw
} //ib
