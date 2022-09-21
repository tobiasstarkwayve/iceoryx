// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 - 2022 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_hoofs/log/platform_building_blocks/console_logger.hpp"
#include "iceoryx_hoofs/cxx/attributes.hpp"
#include "iceoryx_platform/time.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace iox
{
namespace pbb
{
// NOLINTJUSTIFICATION see at declaration in header
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<LogLevel> ConsoleLogger::m_activeLogLevel{LogLevel::INFO};

// NOLINTJUSTIFICATION see at declaration in header
// NOLINTNEXTLINE(hicpp-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays)
thread_local char ConsoleLogger::m_buffer[ConsoleLogger::NULL_TERMINATED_BUFFER_SIZE]{0};

thread_local uint32_t ConsoleLogger::m_bufferWriteIndex{0U};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

LogLevel ConsoleLogger::getLogLevel() noexcept
{
    return m_activeLogLevel.load(std::memory_order_relaxed);
}

// NOLINTJUSTIFICATION member access is required
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void ConsoleLogger::setLogLevel(const LogLevel logLevel) noexcept
{
    m_activeLogLevel.store(logLevel, std::memory_order_relaxed);
}

void ConsoleLogger::createLogMessageHeader(const char* file,
                                           const int line,
                                           const char* function,
                                           LogLevel logLevel) noexcept
{
    timespec timestamp{0, 0};
    if (clock_gettime(CLOCK_REALTIME, &timestamp) != 0)
    {
        timestamp = {0, 0};
        // intentionally do nothing since a timestamp from 01.01.1970 already indicates  an issue with the clock
    }

    time_t time = timestamp.tv_sec;

/// @todo iox-#1345 since this will be part of the platform at one point, we might not be able to handle this via the
/// platform abstraction; re-evaluate this when the move to the platform happens
#if defined(_WIN32)
    // seems to be thread-safe on Windows
    auto* timeInfo = localtime(&time);
#else
    // NOLINTJUSTIFICATION will be initialized with the call to localtime_r in the statement after the declaration
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    struct tm calendarData;
    auto* timeInfo = localtime_r(&time, &calendarData);
#endif

    // NOLINTJUSTIFICATION this is used to get the size for the buffer where strftime writes the local time
    // NOLINTNEXTLINE(hicpp-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays)
    constexpr const char TIME_FORMAT[]{"0000-00-00 00:00:00"};
    constexpr uint32_t NULL_TERMINATION{1};
    constexpr uint32_t YEAR_1M_PROBLEM{2}; // in case iceoryx is still in use, please change to 3
    constexpr auto NULL_TERMINATED_TIMESTAMP_BUFFER_SIZE{ConsoleLogger::bufferSize(TIME_FORMAT) + YEAR_1M_PROBLEM
                                                         + NULL_TERMINATION};

    // NOLINTJUSTIFICATION required for strftime and safe since array bounds are taken into account
    // NOLINTNEXTLINE(hicpp-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays)
    char timestampString[NULL_TERMINATED_TIMESTAMP_BUFFER_SIZE]{0};

    bool timeStampConversionSuccessful{false};
    if (timeInfo != nullptr)
    {
        auto strftimeRetVal =
            strftime(&timestampString[0], NULL_TERMINATED_TIMESTAMP_BUFFER_SIZE, "%Y-%m-%d %H:%M:%S", timeInfo);
        timeStampConversionSuccessful = (strftimeRetVal != 0);
    }

    if (!timeStampConversionSuccessful)
    {
        // this will clearly indicate that something went wrong with the time conversion; no need to abort the log
        // output
        strncpy(&timestampString[0], &TIME_FORMAT[0], ConsoleLogger::bufferSize(TIME_FORMAT));
    }

    constexpr auto MILLISECS_PER_SECOND{1000};
    auto milliseconds = static_cast<int32_t>(timestamp.tv_nsec % MILLISECS_PER_SECOND);

    /// @todo iox-#1345 do we also want to always log the iceoryx version and commit sha? Maybe do that only in
    /// `initLogger` with LogDebug

    /// @todo iox-#1345 add an option to also print file, line and function
    unused(file);
    unused(line);
    unused(function);

    // NOLINTJUSTIFICATION snprintf required to populate char array so that it can be flushed in one piece
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
    auto retVal = snprintf(&m_buffer[0],
                           NULL_TERMINATED_BUFFER_SIZE,
                           "\033[0;90m%s.%03d %s%s\033[m: ",
                           &timestampString[0],
                           milliseconds,
                           logLevelDisplayColor(logLevel),
                           logLevelDisplayText(logLevel));
    if (retVal < 0)
    {
        /// @todo iox-#1345 this path should never be reached since we ensured the correct encoding of the character
        /// conversion specifier; nevertheless, we might want to call the error handler after the error handler
        /// refactoring was merged
    }
    else
    {
        auto stringSizeToLog = static_cast<uint32_t>(retVal);
        if (stringSizeToLog <= BUFFER_SIZE)
        {
            m_bufferWriteIndex = stringSizeToLog;
        }
        else
        {
            /// @todo iox-#1345 currently the buffer is large enough that this does not happen but once the file or
            /// function will also be printed, they might be too long to fit into the buffer and will be truncated; once
            /// that feature is implemented, we need to take care of it
            m_bufferWriteIndex = BUFFER_SIZE;
        }
    }
}

void ConsoleLogger::flush() noexcept
{
    if (std::puts(&m_buffer[0]) < 0)
    {
        /// @todo iox-#1345 printing to the console failed; call the error handler after the error handler refactoring
        /// was merged
    }
    assumeFlushed();
}

// NOLINTJUSTIFICATION member access is required
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
LogBuffer ConsoleLogger::getLogBuffer() const noexcept
{
    return LogBuffer{&m_buffer[0], m_bufferWriteIndex};
}

// NOLINTJUSTIFICATION member access is required
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void ConsoleLogger::assumeFlushed() noexcept
{
    m_buffer[0] = 0;
    m_bufferWriteIndex = 0;
}

// NOLINTJUSTIFICATION member access is required
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void ConsoleLogger::logString(const char* message) noexcept
{
    auto retVal =
        // NOLINTJUSTIFICATION snprintf required to populate char array so that it can be flushed in one piece
        // NOLINTBEGIN(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
        // NOLINTJUSTIFICATION it is ensured that the index cannot be out of bounds
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        snprintf(&m_buffer[m_bufferWriteIndex], NULL_TERMINATED_BUFFER_SIZE - m_bufferWriteIndex, "%s", message);
    // NOLINTEND(cppcoreguidelines-pro-type-vararg,hicpp-vararg)

    if (retVal < 0)
    {
        /// @todo iox-#1345 this path should never be reached since we ensured the correct encoding of the character
        /// conversion specifier; nevertheless, we might want to call the error handler after the error handler
        /// refactoring was merged
    }
    else
    {
        auto stringSizeToLog = static_cast<uint32_t>(retVal);
        auto bufferWriteIndexNext = m_bufferWriteIndex + stringSizeToLog;
        if (bufferWriteIndexNext <= BUFFER_SIZE)
        {
            m_bufferWriteIndex = bufferWriteIndexNext;
        }
        else
        {
            /// @todo iox-#1345 currently we don't support log messages larger than the log buffer and everything larger
            /// that the log buffer will be truncated;
            /// it is intended to flush the buffer and create a new log message later on
            m_bufferWriteIndex = BUFFER_SIZE;
        }
    }
}

void ConsoleLogger::logI64Dec(const int64_t value) noexcept
{
    logArithmetik(value, "%li");
}
void ConsoleLogger::logU64Dec(const uint64_t value) noexcept
{
    logArithmetik(value, "%lu");
}
void ConsoleLogger::logU64Hex(const uint64_t value) noexcept
{
    logArithmetik(value, "%x");
}
void ConsoleLogger::logU64Oct(const uint64_t value) noexcept
{
    logArithmetik(value, "%o");
}

void ConsoleLogger::initLogger(const LogLevel) noexcept
{
    // nothing to do in the base implementation
}
} // namespace pbb
} // namespace iox