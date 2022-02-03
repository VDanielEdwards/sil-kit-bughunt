#include "ib/capi/IntegrationBus.h"
#include "ib/IntegrationBus.hpp"
#include "ib/mw/logging/ILogger.hpp"
#include "ib/mw/sync/all.hpp"
#include "ib/mw/sync/string_utils.hpp"
#include "ib/sim/rpc/all.hpp"

#include "CapiImpl.h"

#include <string>
#include <iostream>
#include <algorithm>
#include <map>
#include <mutex>
#include <cstring>

ib_ReturnCode ib_Rpc_Client_Create(ib_Rpc_Client** out, ib_SimulationParticipant* participant,
                                                      const char* functionName, ib_Rpc_ExchangeFormat* exchangeFormat,
                                                      void* context, ib_Rpc_ResultHandler_t resultHandler)
{
    ASSERT_VALID_OUT_PARAMETER(out);
    ASSERT_VALID_POINTER_PARAMETER(participant);
    ASSERT_VALID_POINTER_PARAMETER(functionName);
    ASSERT_VALID_POINTER_PARAMETER(exchangeFormat);
    ASSERT_VALID_HANDLER_PARAMETER(resultHandler);
    CAPI_ENTER
    {
        std::string strFunctionName(functionName);
        ib::sim::rpc::RpcExchangeFormat cppExchangeFormat{std::string(exchangeFormat->mediaType)};
        auto comAdapter = reinterpret_cast<ib::mw::IComAdapter*>(participant);
        auto rcpClient = comAdapter->CreateRpcClient(functionName, cppExchangeFormat,
            [resultHandler, context, out] (ib::sim::rpc::IRpcClient* client, ib::sim::rpc::IRpcCallHandle* callHandle,
                  const ib::sim::rpc::CallStatus callStatus, const std::vector<uint8_t>& returnData) 
            {
                uint8_t* payloadPointer = NULL;
                if (returnData.size() > 0)
                {
                    payloadPointer = (uint8_t* const) &(returnData[0]);
                }
                const ib_ByteVector cReturnData{payloadPointer, returnData.size()};
                ib_Rpc_CallHandle* cCallHandle = reinterpret_cast<ib_Rpc_CallHandle*>(callHandle);
                ib_Rpc_CallStatus cCallStatus = (ib_Rpc_CallStatus)callStatus;
                resultHandler(context, *out, cCallHandle, cCallStatus, &cReturnData);
            });

        *out = reinterpret_cast<ib_Rpc_Client*>(rcpClient);
        return ib_ReturnCode_SUCCESS;
    }
    CAPI_LEAVE
}


ib_ReturnCode ib_Rpc_Server_Create(ib_Rpc_Server** out, ib_SimulationParticipant* participant,
                                                      const char* functionName, ib_Rpc_ExchangeFormat* exchangeFormat,
                                                      void* context, ib_Rpc_CallHandler_t callHandler)
{
    ASSERT_VALID_OUT_PARAMETER(out);
    ASSERT_VALID_POINTER_PARAMETER(participant);
    ASSERT_VALID_POINTER_PARAMETER(functionName);
    ASSERT_VALID_POINTER_PARAMETER(exchangeFormat);
    ASSERT_VALID_HANDLER_PARAMETER(callHandler);
    CAPI_ENTER
    {
        std::string strFunctionName(functionName);
        ib::sim::rpc::RpcExchangeFormat cppExchangeFormat{std::string(exchangeFormat->mediaType)};
        auto comAdapter = reinterpret_cast<ib::mw::IComAdapter*>(participant);
        auto rcpServer = comAdapter->CreateRpcServer(functionName, cppExchangeFormat,
            [callHandler, context, out](ib::sim::rpc::IRpcServer* server, ib::sim::rpc::IRpcCallHandle* callHandle,
                  const std::vector<uint8_t>& argumentData)
            {
                uint8_t* payloadPointer = NULL;
                if (argumentData.size() > 0)
                {
                    payloadPointer = (uint8_t* const)&(argumentData[0]);
                }
                const ib_ByteVector cArgumentData{payloadPointer, argumentData.size()};
                ib_Rpc_CallHandle* cCallHandle = reinterpret_cast<ib_Rpc_CallHandle*>(callHandle);
                callHandler(context, *out, cCallHandle, &cArgumentData);
            });

        *out = reinterpret_cast<ib_Rpc_Server*>(rcpServer);
        return ib_ReturnCode_SUCCESS;
    }
    CAPI_LEAVE
}

ib_ReturnCode ib_Rpc_Client_Call(ib_Rpc_Client* self, ib_Rpc_CallHandle** outHandle,
                                                    const ib_ByteVector* argumentData)
{
    ASSERT_VALID_POINTER_PARAMETER(self);
    ASSERT_VALID_OUT_PARAMETER(outHandle);
    ASSERT_VALID_POINTER_PARAMETER(argumentData);
    CAPI_ENTER
    {
        auto cppClient = reinterpret_cast<ib::sim::rpc::IRpcClient*>(self);
        auto cppCallHandle = cppClient->Call(std::vector<uint8_t>(&(argumentData->pointer[0]), &(argumentData->pointer[0]) + argumentData->size));
        *outHandle = reinterpret_cast<ib_Rpc_CallHandle*>(cppCallHandle);
        return ib_ReturnCode_SUCCESS;
    }
    CAPI_LEAVE
}



ib_ReturnCode ib_Rpc_Server_SubmitResult(ib_Rpc_Server* self, ib_Rpc_CallHandle* callHandle,
                                                            const ib_ByteVector* returnData)
{
    ASSERT_VALID_POINTER_PARAMETER(self);
    ASSERT_VALID_POINTER_PARAMETER(callHandle);
    ASSERT_VALID_POINTER_PARAMETER(returnData);
    CAPI_ENTER
    {
        auto cppServer = reinterpret_cast<ib::sim::rpc::IRpcServer*>(self);
        auto cppCallHandle = reinterpret_cast<ib::sim::rpc::IRpcCallHandle*>(callHandle);
        auto cppReturnData = std::vector<uint8_t>(&(returnData->pointer[0]), &(returnData->pointer[0]) + returnData->size);
        cppServer->SubmitResult(cppCallHandle, cppReturnData);
        return ib_ReturnCode_SUCCESS;
    }
    CAPI_LEAVE
}
