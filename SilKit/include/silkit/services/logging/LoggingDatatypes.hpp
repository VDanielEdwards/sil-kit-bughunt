// Copyright (c) Vector Informatik GmbH. All rights reserved.

#pragma once

#include <chrono>
#include <string>

namespace SilKit {
namespace Services {
namespace Logging {

using log_clock = std::chrono::system_clock;

/*! \brief Information level of a log message
 */
enum class Level : uint32_t
{
    Trace = 0, //!< Detailed debug-level messages
    Debug = 1, //!< Normal debug-level messages
    Info = 2, //!< Informational content
    Warn = 3, //!< Warnings
    Error = 4, //!< Non-critical errors
    Critical = 5, //!< Critical errors
    Off = 0xffffffff //!< Logging is disabled
};

/*! \brief The source location that a log entry refers to
 */
struct SourceLoc
{
    std::string filename;
    uint32_t line;
    std::string funcname;
};

/*! \brief A log entry
 */
struct LogMsg
{
    std::string logger_name;
    Level level{Level::Off};
    log_clock::time_point time;
    SourceLoc source;
    std::string payload;
};

// ================================================================================
//  Inline Implementations
// ================================================================================
inline bool operator==(const SourceLoc& lhs, const SourceLoc& rhs)
{
    return lhs.filename == rhs.filename
        && lhs.line == rhs.line
        && lhs.funcname == rhs.funcname;
}

inline bool operator==(const LogMsg& lhs, const LogMsg& rhs)
{
    return lhs.logger_name == rhs.logger_name
        && lhs.level == rhs.level
        && lhs.time == rhs.time
        && lhs.source == rhs.source
        && lhs.payload == rhs.payload;
}

} // namespace Logging
} // namespace Services
} // namespace SilKit
