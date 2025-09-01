

#include "CLogger.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

namespace FauxDB
{

CLogger::CLogger(const CServerConfig& config)
    : config_(config), logFile_(""), errorLogFile_(""), consoleOutput_(true),
      fileOutput_(true), maxLogFileSize_(10485760), logRotationCount_(5),
      timestampFormat_("%Y-%m-%d %H:%M:%S"), logLevel_(CLogLevel::INFO),
      initialized_(false), stopAsyncLogger_(false)
{
}

CLogger::~CLogger()
{
    if (fileStream_ && fileStream_->is_open())
    {
        fileStream_->close();
    }
    if (errorStream_ && errorStream_->is_open())
    {
        errorStream_->close();
    }
}

void CLogger::log(CLogLevel level, const std::string& message)
{
    if (level < logLevel_)
    {
        return;
    }

    std::string formattedMessage = formatMessage(level, message);

    if (consoleOutput_)
    {
        writeToConsole(formattedMessage);
    }

    if (fileOutput_ && fileStream_ && fileStream_->is_open())
    {
        writeToFile(level, formattedMessage);
    }
}

void CLogger::setLogLevel(CLogLevel level)
{
    logLevel_ = level;
}

CLogLevel CLogger::getLogLevel() const noexcept
{
    return logLevel_;
}

std::error_code CLogger::initialize()
{
    if (initialized_)
    {
        return std::error_code();
    }

    try
    {
        if (fileOutput_ && !logFile_.empty())
        {
            fileStream_ =
                std::make_unique<std::ofstream>(logFile_, std::ios::app);
            if (!fileStream_->is_open())
            {
                return std::make_error_code(std::errc::io_error);
            }
        }

        if (!errorLogFile_.empty())
        {
            errorStream_ =
                std::make_unique<std::ofstream>(errorLogFile_, std::ios::app);
            if (!errorStream_->is_open())
            {
                return std::make_error_code(std::errc::io_error);
            }
        }

        initialized_ = true;
        return std::error_code();
    }
    catch (const std::exception& e)
    {
        return std::make_error_code(std::errc::io_error);
    }
}

void CLogger::shutdown() noexcept
{
    if (fileStream_ && fileStream_->is_open())
    {
        fileStream_->close();
    }
    if (errorStream_ && errorStream_->is_open())
    {
        errorStream_->close();
    }
    initialized_ = false;
}

void CLogger::setLogFile(const std::string& filename)
{
    logFile_ = filename;
}

void CLogger::setErrorLogFile(const std::string& filename)
{
    errorLogFile_ = filename;
}

void CLogger::enableConsoleOutput(bool enable)
{
    consoleOutput_ = enable;
}

void CLogger::enableFileOutput(bool enable)
{
    fileOutput_ = enable;
}

void CLogger::setMaxLogFileSize(size_t maxSize)
{
    maxLogFileSize_ = maxSize;
}

void CLogger::setLogRotationCount(size_t count)
{
    logRotationCount_ = count;
}

void CLogger::setTimestampFormat(const std::string& format)
{
    timestampFormat_ = format;
}

void CLogger::writeToConsole(const std::string& message)
{

    if (consoleOutput_)
    {
        std::cerr << message << std::endl;
    }
}

void CLogger::writeToFile(CLogLevel /* level */, const std::string& message)
{
    if (fileStream_ && fileStream_->is_open())
    {
        *fileStream_ << message << std::endl;
        fileStream_->flush();
    }
}

std::string CLogger::formatMessage(CLogLevel level, const std::string& message)
{
    std::stringstream ss;
    ss << "[" << getTimestamp() << "] [" << getLevelString(level) << "] "
       << message;
    return ss.str();
}

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

std::string CLogger::getTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    std::stringstream ss;
    ss << std::put_time(&tm, timestampFormat_.c_str());
    return ss.str();
}

bool CLogger::shouldLog(CLogLevel level) const noexcept
{
    return level >= logLevel_;
}

} // namespace FauxDB
