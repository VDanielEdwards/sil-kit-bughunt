// Copyright (c) Vector Informatik GmbH. All rights reserved.
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "silkit/capi/SilKit.h"
#include "silkit/core/logging/all.hpp"

namespace {
    using namespace SilKit::Core::Logging;

    class MockLogger : public SilKit::Core::Logging::ILogger
    {
    public:
        MOCK_METHOD2(Log, void(Level, const std::string&));
        MOCK_METHOD1(Trace, void(const std::string&));
        MOCK_METHOD1(Debug, void(const std::string&));
        MOCK_METHOD1(Info, void(const std::string&));
        MOCK_METHOD1(Warn, void(const std::string&));
        MOCK_METHOD1(Error, void(const std::string&));
        MOCK_METHOD1(Critical, void(const std::string&));

        MOCK_CONST_METHOD1(ShouldLog, bool(Level level));
    };

    class CapiLoggerTest : public testing::Test
    {
    public:
        MockLogger mockLogger;
        CapiLoggerTest()
        {
        }
    };

    TEST_F(CapiLoggerTest, logger_function_mapping)
    {
        SilKit_ReturnCode returnCode;

        EXPECT_CALL(mockLogger, Log(Level::Off, "Test message")).Times(testing::Exactly(1));
        returnCode = SilKit_Logger_Log((SilKit_Logger*)&mockLogger, SilKit_LoggingLevel_Off, "Test message");
        EXPECT_EQ(returnCode, SilKit_ReturnCode_SUCCESS);
    }

    TEST_F(CapiLoggerTest, logger_nullpointer_params)
    {
        SilKit_ReturnCode returnCode;

        returnCode = SilKit_Logger_Log(nullptr, SilKit_LoggingLevel_Off, "Test message");
        EXPECT_EQ(returnCode, SilKit_ReturnCode_BADPARAMETER);

        returnCode = SilKit_Logger_Log((SilKit_Logger*)&mockLogger, SilKit_LoggingLevel_Off, nullptr);
        EXPECT_EQ(returnCode, SilKit_ReturnCode_BADPARAMETER);
    }

    TEST_F(CapiLoggerTest, logger_enum_mappings)
    {
        SilKit_ReturnCode returnCode;

        EXPECT_CALL(mockLogger, Log(Level::Off, "Test message")).Times(testing::Exactly(1));
        returnCode = SilKit_Logger_Log((SilKit_Logger*)&mockLogger, SilKit_LoggingLevel_Off, "Test message");
        EXPECT_EQ(returnCode, SilKit_ReturnCode_SUCCESS);

        EXPECT_CALL(mockLogger, Log(Level::Trace, "Trace message")).Times(testing::Exactly(1));
        returnCode = SilKit_Logger_Log((SilKit_Logger*)&mockLogger, SilKit_LoggingLevel_Trace, "Trace message");
        EXPECT_EQ(returnCode, SilKit_ReturnCode_SUCCESS);

        EXPECT_CALL(mockLogger, Log(Level::Debug, "Debug message")).Times(testing::Exactly(1));
        returnCode = SilKit_Logger_Log((SilKit_Logger*)&mockLogger, SilKit_LoggingLevel_Debug, "Debug message");
        EXPECT_EQ(returnCode, SilKit_ReturnCode_SUCCESS);

        EXPECT_CALL(mockLogger, Log(Level::Info, "Info message")).Times(testing::Exactly(1));
        returnCode = SilKit_Logger_Log((SilKit_Logger*)&mockLogger, SilKit_LoggingLevel_Info, "Info message");
        EXPECT_EQ(returnCode, SilKit_ReturnCode_SUCCESS);

        EXPECT_CALL(mockLogger, Log(Level::Warn, "Warn message")).Times(testing::Exactly(1));
        returnCode = SilKit_Logger_Log((SilKit_Logger*)&mockLogger, SilKit_LoggingLevel_Warn, "Warn message");
        EXPECT_EQ(returnCode, SilKit_ReturnCode_SUCCESS);

        EXPECT_CALL(mockLogger, Log(Level::Error, "Error message")).Times(testing::Exactly(1));
        returnCode = SilKit_Logger_Log((SilKit_Logger*)&mockLogger, SilKit_LoggingLevel_Error, "Error message");
        EXPECT_EQ(returnCode, SilKit_ReturnCode_SUCCESS);

        EXPECT_CALL(mockLogger, Log(Level::Critical, "Critical message")).Times(testing::Exactly(1));
        returnCode = SilKit_Logger_Log((SilKit_Logger*)&mockLogger, SilKit_LoggingLevel_Critical, "Critical message");
        EXPECT_EQ(returnCode, SilKit_ReturnCode_SUCCESS);
    }
}
