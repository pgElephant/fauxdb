/*-------------------------------------------------------------------------
 *
 * CLogger.cpp
 *		  Logging system implementation for FauxDB
 *
 * Provides structured logging with multiple output targets, log rotation,
 * and configurable formatting.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		  src/CLogger.cpp
 *
 *-------------------------------------------------------------------------
 */

#include "CLogger.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>

using namespace std;

namespace FauxDB
{

/*
 * CLogger constructor
 *		Initialize logger with configuration
 */
CLogger::CLogger(const CServerConfig& config)
    : config_(config), logFile_(""), errorLogFile_(""), consoleOutput_(true),
      fileOutput_(true), maxLogFileSize_(10485760), logRotationCount_(5),
      timestampFormat_("%Y-%m-%d %H:%M:%S"), logLevel_(CLogLevel::INFO),
      initialized_(false), stopAsyncLogger_(false)
{
}

/*
 * CLogger destructor
 *		Clean up open file streams
 */
CLogger::~CLogger()
{
    if (fileStream_ && fileStream_->is_open())
        fileStream_->close();
    if (errorStream_ && errorStream_->is_open())
        errorStream_->close();
}

/*
 * log
 *		Main logging function - write message if level is sufficient
 */
void CLogger::log(CLogLevel level, const std::string& message)
{
    std::string formattedMessage;

    if (level < logLevel_)
        return;

    formattedMessage = formatMessage(level, message);

    if (consoleOutput_)
        writeToConsole(formattedMessage);

    if (fileOutput_ && fileStream_ && fileStream_->is_open())
        writeToFile(level, formattedMessage);
}

/*
 * setLogLevel
 *		Set minimum log level for output
 */
void CLogger::setLogLevel(CLogLevel level)
{
    logLevel_ = level;
}

/*
 * getLogLevel
 *		Return current log level
 */
CLogLevel CLogger::getLogLevel() const noexcept
{
    return logLevel_;
}

/*
 * initialize
 *		Set up file streams and prepare logger for use
 */
std::error_code CLogger::initialize()
{
    if (initialized_)
        return std::error_code();

    try
    {
        if (fileOutput_ && !logFile_.empty())
        {
            fileStream_ =
                std::make_unique<std::ofstream>(logFile_, std::ios::app);
            if (!fileStream_->is_open())
                return std::make_error_code(std::errc::io_error);
        }

        if (!errorLogFile_.empty())
        {
            errorStream_ =
                std::make_unique<std::ofstream>(errorLogFile_, std::ios::app);
            if (!errorStream_->is_open())
                return std::make_error_code(std::errc::io_error);
        }

        initialized_ = true;
        return std::error_code();
    }
    catch (const std::exception& e)
    {
        return std::make_error_code(std::errc::io_error);
    }
}

/*
 * shutdown
 *		Close file streams and clean up resources
 */
void CLogger::shutdown() noexcept
{
    if (fileStream_ && fileStream_->is_open())
        fileStream_->close();
    if (errorStream_ && errorStream_->is_open())
        errorStream_->close();
    initialized_ = false;
}

/*
 * setLogFile
 *		Set main log file path
 */
void CLogger::setLogFile(const std::string& filename)
{
    logFile_ = filename;
}

/*
 * setErrorLogFile
 *		Set error log file path
 */
void CLogger::setErrorLogFile(const std::string& filename)
{
    errorLogFile_ = filename;
}

/*
 * enableConsoleOutput
 *		Enable or disable console output
 */
void CLogger::enableConsoleOutput(bool enable)
{
    consoleOutput_ = enable;
}

/*
 * enableFileOutput
 *		Enable or disable file output
 */
void CLogger::enableFileOutput(bool enable)
{
    fileOutput_ = enable;
}

/*
 * setMaxLogFileSize
 *		Set maximum log file size for rotation
 */
void CLogger::setMaxLogFileSize(size_t maxSize)
{
    maxLogFileSize_ = maxSize;
}

/*
 * setLogRotationCount
 *		Set number of rotated log files to keep
 */
void CLogger::setLogRotationCount(size_t count)
{
    logRotationCount_ = count;
}

/*
 * setTimestampFormat
 *		Set timestamp format string
 */
void CLogger::setTimestampFormat(const std::string& format)
{
    timestampFormat_ = format;
}

/*
 * writeToConsole
 *		Write formatted message to console
 */
void CLogger::writeToConsole(const std::string& message)
{
    if (consoleOutput_)
        std::cerr << message << std::endl;
}

/*
 * writeToFile
 *		Write formatted message to log file
 */
void CLogger::writeToFile(CLogLevel /* level */, const std::string& message)
{
    if (fileStream_ && fileStream_->is_open())
    {
        *fileStream_ << message << std::endl;
        fileStream_->flush();
    }
}

/*
 * formatMessage
 *		Format log message with timestamp, level, and metadata
 */
std::string CLogger::formatMessage(CLogLevel level, const std::string& message)
{
    std::stringstream ss;
    int pid = static_cast<int>(getpid());
    const char* user = getenv("USER");
    std::string username = user ? user : "unknown";
    std::string timestamp = getTimestamp();
    std::string component =
        config_.serverName.empty() ? "fauxdb" : config_.serverName;

    /* ANSI color codes */
    const char* green = "\033[32m";
    const char* red = "\033[31m";
    const char* blue = "\033[34m";
    const char* reset = "\033[0m";

    std::string symbol;
    const char* color = reset;

    if (level == CLogLevel::ERROR || level == CLogLevel::FATAL)
    {
        symbol = "\u2717"; /* ✗ */
        color = red;
    }
    else if (level == CLogLevel::INFO)
    {
        symbol = "\u2713"; /* ✓ */
        color = green;
    }
    else if (level == CLogLevel::DEBUG)
    {
        symbol = "\u2139"; /* ℹ */
        color = blue;
    }
    else
    {
        symbol = "\u2713";
        color = reset;
    }

    /* Only add component if not already present at start of message */
    std::string formattedMessage = message;
    std::string prefix = component + ": ";
    if (formattedMessage.rfind(prefix, 0) == 0)
    {
        ss << color << symbol << " - " << pid << "  " << username << " "
           << timestamp << " " << formattedMessage << reset;
    }
    else
    {
        ss << color << symbol << " - " << pid << "  " << username << " "
           << timestamp << " " << component << ": " << formattedMessage
           << reset;
    }
    return ss.str();
}

/*
 * getLevelString
 *		Convert log level enum to string representation
 */
std::string CLogger::getLevelString(CLogLevel level) const
{
    switch (level)
    {
    case CLogLevel::DEBUG:
        return "DEBUG";
    case CLogLevel::INFO:
        return "INFO";
    case CLogLevel::WARN:
        return "WARN";
    case CLogLevel::ERROR:
        return "ERROR";
    case CLogLevel::FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

/*
 * getTimestamp
 *		Generate formatted timestamp string
 */
std::string CLogger::getTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    std::stringstream ss;

    ss << std::put_time(&tm, timestampFormat_.c_str());
    return ss.str();
}

/*
 * shouldLog
 *		Check if message should be logged based on level
 */
bool CLogger::shouldLog(CLogLevel level) const noexcept
{
    return level >= logLevel_;
}

} /* namespace FauxDB */
