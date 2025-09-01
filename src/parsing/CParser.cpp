

#include "CParser.hpp"

#include <cstring>
#include <stdexcept>

using namespace std;
namespace FauxDB
{

CParser::CParser() : parseBuffer_(), isInitialized_(false)
{
    initializeDefaults();
}

CParser::~CParser() = default;

void CParser::initializeDefaults()
{
    maxBufferSize_ = 1024 * 1024;               // 1MB default
    timeout_ = std::chrono::milliseconds(5000); // 5 second default
    strictMode_ = false;
    debugMode_ = false;
    lastStatus_ = CParserStatus::Success;
    startTime_ = std::chrono::steady_clock::now();
    endTime_ = startTime_;
}

bool CParser::initialize()
{
    try
    {
        parseBuffer_.clear();
        buffer_.clear();
        buffer_.reserve(maxBufferSize_);
        isInitialized_ = true;
        clearErrors();
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Initialization failed: " + std::string(e.what()),
                 CParserStatus::Error);
        isInitialized_ = false;
        return false;
    }
}

void CParser::shutdown()
{
    isInitialized_ = false;
    parseBuffer_.clear();
    buffer_.clear();
    stopTimer();
}

bool CParser::isInitialized() const
{
    return isInitialized_;
}

bool CParser::parseMessage(const std::vector<uint8_t>& data)
{
    if (!isInitialized_)
    {
        return false;
    }

    try
    {
        parseBuffer_ = data;
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Parse message failed: " + std::string(e.what()),
                 CParserStatus::Error);
        return false;
    }
}

std::vector<uint8_t> CParser::getParseBuffer() const
{
    return parseBuffer_;
}

void CParser::clearParseBuffer()
{
    parseBuffer_.clear();
}

bool CParser::hasCompleteMessage() const
{
    return parseBuffer_.size() >= 16;
}

std::vector<uint8_t> CParser::extractMessage()
{
    if (!hasCompleteMessage())
    {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> message = parseBuffer_;
    parseBuffer_.clear();
    return message;
}

void CParser::setMaxBufferSize(std::size_t maxSize)
{
    maxBufferSize_ = maxSize;
    if (buffer_.capacity() > maxBufferSize_)
    {
        buffer_.reserve(maxBufferSize_);
    }
}

void CParser::setTimeout(std::chrono::milliseconds timeout)
{
    timeout_ = timeout;
}

void CParser::setStrictMode(bool strict)
{
    strictMode_ = strict;
}

void CParser::setDebugMode(bool debug)
{
    debugMode_ = debug;
}

std::string CParser::getLastError() const
{
    return lastError_;
}

CParserStatus CParser::getLastStatus() const
{
    return lastStatus_;
}

void CParser::clearErrors()
{
    lastError_.clear();
    lastStatus_ = CParserStatus::Success;
}

void CParser::setError(const std::string& error, CParserStatus status)
{
    lastError_ = error;
    lastStatus_ = status;
}

bool CParser::checkBufferSize(std::size_t requiredSize)
{
    return requiredSize <= maxBufferSize_;
}

void CParser::resizeBuffer(std::size_t newSize)
{
    if (newSize <= maxBufferSize_)
    {
        buffer_.resize(newSize);
    }
}

bool CParser::validateBuffer()
{
    return buffer_.size() <= maxBufferSize_;
}

bool CParser::checkTimeout() const
{
    auto now = std::chrono::steady_clock::now();
    return (now - startTime_) > timeout_;
}

void CParser::startTimer()
{
    startTime_ = std::chrono::steady_clock::now();
}

void CParser::stopTimer()
{
    endTime_ = std::chrono::steady_clock::now();
}

void CParser::reset()
{
    parseBuffer_.clear();
    buffer_.clear();
    clearErrors();
    stopTimer();
}

void CParser::clear()
{
    parseBuffer_.clear();
    buffer_.clear();
    clearErrors();
    stopTimer();
}

bool CParser::isReady() const
{
    return isInitialized_ && lastStatus_ != CParserStatus::Error;
}

std::size_t CParser::getBufferSize() const
{
    return buffer_.size();
}

void CParser::cleanupBuffer()
{
    if (buffer_.capacity() > maxBufferSize_)
    {
        buffer_.shrink_to_fit();
    }
}

} // namespace FauxDB
