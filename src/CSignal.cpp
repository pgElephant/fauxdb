/*-------------------------------------------------------------------------
 *
 * CSignal.cpp
 *      Signal handling utilities for FauxDB server.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */


#include "CSignal.hpp"

#include "CLogger.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

namespace FauxDB
{

std::unordered_map<int, CSignal*> CSignal::g_signalInstances;
std::mutex CSignal::g_signalMutex;

CSignal::CSignal() : initialized_(false), monitoringEnabled_(false)
{
    initializeDefaults();
}

CSignal::~CSignal()
{
    shutdown();
}

void CSignal::initializeDefaults()
{
    config_ = CSignalConfig{};
    handlers_.clear();
    defaultHandlers_.clear();
    blockedSignals_.clear();
    signalCounts_.clear();
    lastError_.clear();
}

bool CSignal::initialize(const CSignalConfig& config)
{
    if (initialized_)
    {
        return true;
    }

    try
    {
        config_ = config;
        initialized_ = true;
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

void CSignal::shutdown()
{
    if (!initialized_)
    {
        return;
    }

    try
    {
        handlers_.clear();
        defaultHandlers_.clear();
        blockedSignals_.clear();
        initialized_ = false;
    }
    catch (const std::exception& e)
    {
        setError("Shutdown failed: " + std::string(e.what()));
    }
}

bool CSignal::isInitialized() const
{
    return initialized_;
}

bool CSignal::registerHandler(CSignalType signalType, CSignalHandler handler)
{
    if (!initialized_ || !validateSignalType(signalType))
    {
        return false;
    }

    try
    {
        handlers_[signalType] = handler;
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Failed to register handler: " + std::string(e.what()));
        return false;
    }
}

bool CSignal::unregisterHandler(CSignalType signalType)
{
    if (!initialized_ || !validateSignalType(signalType))
    {
        return false;
    }

    try
    {
        handlers_.erase(signalType);
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Failed to unregister handler: " + std::string(e.what()));
        return false;
    }
}

bool CSignal::hasHandler(CSignalType signalType) const
{
    return handlers_.find(signalType) != handlers_.end();
}

CSignalHandler CSignal::getHandler(CSignalType signalType) const
{
    auto it = handlers_.find(signalType);
    return (it != handlers_.end()) ? it->second : CSignalHandler{};
}

bool CSignal::blockSignal(CSignalType signalType)
{
    if (!initialized_ || !validateSignalType(signalType))
    {
        return false;
    }

    try
    {
        blockedSignals_[signalType] = true;
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Failed to block signal: " + std::string(e.what()));
        return false;
    }
}

bool CSignal::unblockSignal(CSignalType signalType)
{
    if (!initialized_ || !validateSignalType(signalType))
    {
        return false;
    }

    try
    {
        blockedSignals_[signalType] = false;
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Failed to unblock signal: " + std::string(e.what()));
        return false;
    }
}

bool CSignal::isSignalBlocked(CSignalType signalType) const
{
    auto it = blockedSignals_.find(signalType);
    return (it != blockedSignals_.end()) ? it->second : false;
}

bool CSignal::ignoreSignal(CSignalType signalType)
{
    if (!initialized_ || !validateSignalType(signalType))
    {
        return false;
    }

    try
    {
        signal(static_cast<int>(signalType), SIG_IGN);
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Failed to ignore signal: " + std::string(e.what()));
        return false;
    }
}

bool CSignal::resetSignal(CSignalType signalType)
{
    if (!initialized_ || !validateSignalType(signalType))
    {
        return false;
    }

    try
    {
        signal(static_cast<int>(signalType), SIG_DFL);
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Failed to reset signal: " + std::string(e.what()));
        return false;
    }
}

bool CSignal::sendSignal(int processId, CSignalType signalType)
{
    if (!initialized_ || !validateSignalType(signalType) ||
        !isValidProcessId(processId))
    {
        return false;
    }

    try
    {
        if (kill(processId, static_cast<int>(signalType)) == 0)
        {
            updateSignalCount(signalType);
            return true;
        }
        else
        {
            setError("Failed to send signal: " +
                     std::string(std::strerror(errno)));
            return false;
        }
    }
    catch (const std::exception& e)
    {
        setError("Failed to send signal: " + std::string(e.what()));
        return false;
    }
}

bool CSignal::sendSignalToSelf(CSignalType signalType)
{
    return sendSignal(getpid(), signalType);
}

bool CSignal::raiseSignal(CSignalType signalType)
{
    if (!initialized_ || !validateSignalType(signalType))
    {
        return false;
    }

    try
    {
        raise(static_cast<int>(signalType));
        updateSignalCount(signalType);
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Failed to raise signal: " + std::string(e.what()));
        return false;
    }
}

CSignalInfo CSignal::getSignalInfo(CSignalType signalType) const
{
    CSignalInfo info;
    info.type = signalType;
    info.signalNumber = static_cast<int>(signalType);
    info.name = getSignalName(signalType);
    info.description = getSignalDescription(signalType);
    info.isFatal = isSignalFatal(signalType);
    info.isIgnorable = isSignalIgnorable(signalType);
    return info;
}

std::string CSignal::getSignalName(CSignalType signalType) const
{
    return SignalUtils::getSignalName(static_cast<int>(signalType));
}

std::string CSignal::getSignalDescription(CSignalType signalType) const
{
    return SignalUtils::getSignalDescription(static_cast<int>(signalType));
}

bool CSignal::isSignalFatal(CSignalType signalType) const
{
    switch (signalType)
    {
    case CSignalType::SegmentationFault:
    case CSignalType::FloatingPoint:
    case CSignalType::IllegalInstruction:
    case CSignalType::BusError:
    case CSignalType::Abort:
        return true;
    default:
        return false;
    }
}

bool CSignal::isSignalIgnorable(CSignalType signalType) const
{
    switch (signalType)
    {
    case CSignalType::Stop:
    case CSignalType::TerminalStop:
    case CSignalType::TerminalInput:
    case CSignalType::TerminalOutput:
        return false;
    default:
        return true;
    }
}

bool CSignal::enableSignalMonitoring(bool enabled)
{
    if (!initialized_)
    {
        return false;
    }

    monitoringEnabled_ = enabled;
    return true;
}

bool CSignal::isSignalMonitoringEnabled() const
{
    return monitoringEnabled_;
}

std::vector<CSignalInfo> CSignal::getActiveSignals() const
{
    std::vector<CSignalInfo> activeSignals;
    for (const auto& [signalType, count] : signalCounts_)
    {
        if (count > 0)
        {
            activeSignals.push_back(getSignalInfo(signalType));
        }
    }
    return activeSignals;
}

std::vector<CSignalInfo> CSignal::getBlockedSignals() const
{
    std::vector<CSignalInfo> blockedSignals;
    for (const auto& [signalType, blocked] : blockedSignals_)
    {
        if (blocked)
        {
            blockedSignals.push_back(getSignalInfo(signalType));
        }
    }
    return blockedSignals;
}

size_t CSignal::getSignalCount(CSignalType signalType) const
{
    auto it = signalCounts_.find(signalType);
    return (it != signalCounts_.end()) ? it->second : 0;
}

void CSignal::resetSignalCount(CSignalType signalType)
{
    signalCounts_[signalType] = 0;
}

void CSignal::resetAllSignalCounts()
{
    signalCounts_.clear();
}

std::string CSignal::getSignalStatistics() const
{
    std::stringstream ss;
    ss << "Signal Statistics:\n";
    for (const auto& [signalType, count] : signalCounts_)
    {
        ss << "  " << getSignalName(signalType) << ": " << count << "\n";
    }
    return ss.str();
}

void CSignal::setConfig(const CSignalConfig& config)
{
    config_ = config;
}

CSignalConfig CSignal::getConfig() const
{
    return config_;
}

bool CSignal::validateConfig(const CSignalConfig& config) const
{
    // Basic validation - can be extended
    return !config.logFile.empty() || config.logFile.empty();
}

std::string CSignal::getLastError() const
{
    return lastError_;
}

void CSignal::clearErrors()
{
    lastError_.clear();
}

bool CSignal::hasErrors() const
{
    return !lastError_.empty();
}

void CSignal::setError(const std::string& error) const
{
    lastError_ = error;
}

bool CSignal::validateSignalType(CSignalType signalType) const
{
    int sigNum = static_cast<int>(signalType);
    return sigNum >= 1 && sigNum <= 64; // Valid signal range
}

bool CSignal::installSystemHandler(CSignalType signalType)
{
    if (!initialized_ || !validateSignalType(signalType))
    {
        return false;
    }

    try
    {
        signal(static_cast<int>(signalType), staticSignalHandler);
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Failed to install system handler: " + std::string(e.what()));
        return false;
    }
}

bool CSignal::restoreSystemHandler(CSignalType signalType)
{
    if (!initialized_ || !validateSignalType(signalType))
    {
        return false;
    }

    try
    {
        signal(static_cast<int>(signalType), SIG_DFL);
        return true;
    }
    catch (const std::exception& e)
    {
        setError("Failed to restore system handler: " + std::string(e.what()));
        return false;
    }
}

void CSignal::handleSystemSignal(int signalNumber)
{
    CSignalType signalType = static_cast<CSignalType>(signalNumber);
    updateSignalCount(signalType);
    logSignal(signalType, "Received");

    auto handler = findHandler(signalType);
    if (handler)
    {
        handler(signalType, signalNumber);
    }
}

void CSignal::logSignal(CSignalType signalType, const std::string& action)
{
    if (monitoringEnabled_ && logger_)
    {
        logger_->log(CLogLevel::INFO,
                     getSignalName(signalType) + " - " + action);
    }
}

void CSignal::setLogger(std::shared_ptr<CLogger> logger)
{
    logger_ = logger;
}

void CSignal::updateSignalCount(CSignalType signalType)
{
    signalCounts_[signalType]++;
}

void CSignal::staticSignalHandler(int signalNumber)
{
    std::lock_guard<std::mutex> lock(g_signalMutex);
    auto instance = findGlobalInstance(signalNumber);
    if (instance)
    {
        instance->handleSystemSignal(signalNumber);
    }
}

bool CSignal::isValidProcessId(int processId) const
{
    return processId > 0 && processId <= 999999;
}

std::string CSignal::getSignalString(CSignalType signalType) const
{
    return std::to_string(static_cast<int>(signalType));
}

void CSignal::cleanupState()
{
    handlers_.clear();
    defaultHandlers_.clear();
    blockedSignals_.clear();
    signalCounts_.clear();
}

CSignalHandler CSignal::findHandler(CSignalType signalType) const
{
    auto it = handlers_.find(signalType);
    return (it != handlers_.end()) ? it->second : CSignalHandler{};
}

// Static global instance management
void CSignal::registerGlobalInstance(int signalNumber, CSignal* instance)
{
    std::lock_guard<std::mutex> lock(g_signalMutex);
    g_signalInstances[signalNumber] = instance;
}

void CSignal::unregisterGlobalInstance(int signalNumber)
{
    std::lock_guard<std::mutex> lock(g_signalMutex);
    g_signalInstances.erase(signalNumber);
}

CSignal* CSignal::findGlobalInstance(int signalNumber)
{
    std::lock_guard<std::mutex> lock(g_signalMutex);
    auto it = g_signalInstances.find(signalNumber);
    return (it != g_signalInstances.end()) ? it->second : nullptr;
}

// SignalUtils namespace implementation
namespace SignalUtils
{

std::string getSignalName(int signalNumber)
{
    switch (signalNumber)
    {
    case SIGINT:
        return "SIGINT";
    case SIGTERM:
        return "SIGTERM";
    case SIGQUIT:
        return "SIGQUIT";
    case SIGABRT:
        return "SIGABRT";
    case SIGSEGV:
        return "SIGSEGV";
    case SIGFPE:
        return "SIGFPE";
    case SIGILL:
        return "SIGILL";
    case SIGBUS:
        return "SIGBUS";
    case SIGUSR1:
        return "SIGUSR1";
    case SIGUSR2:
        return "SIGUSR2";
    case SIGPIPE:
        return "SIGPIPE";
    case SIGALRM:
        return "SIGALRM";
    case SIGCHLD:
        return "SIGCHLD";
    case SIGCONT:
        return "SIGCONT";
    case SIGSTOP:
        return "SIGSTOP";
    case SIGTSTP:
        return "SIGTSTP";
    case SIGTTIN:
        return "SIGTTIN";
    case SIGTTOU:
        return "SIGTTOU";
    default:
        return "UNKNOWN";
    }
}

std::string getSignalDescription(int signalNumber)
{
    switch (signalNumber)
    {
    case SIGINT:
        return "Interrupt from keyboard";
    case SIGTERM:
        return "Termination request";
    case SIGQUIT:
        return "Quit from keyboard";
    case SIGABRT:
        return "Abort signal from abort()";
    case SIGSEGV:
        return "Invalid memory reference";
    case SIGFPE:
        return "Floating point exception";
    case SIGILL:
        return "Illegal instruction";
    case SIGBUS:
        return "Bus error";
    case SIGUSR1:
        return "User defined signal 1";
    case SIGUSR2:
        return "User defined signal 2";
    case SIGPIPE:
        return "Broken pipe";
    case SIGALRM:
        return "Timer signal";
    case SIGCHLD:
        return "Child stopped or terminated";
    case SIGCONT:
        return "Continue if stopped";
    case SIGSTOP:
        return "Stop process";
    case SIGTSTP:
        return "Stop typed at terminal";
    case SIGTTIN:
        return "Terminal input for background process";
    case SIGTTOU:
        return "Terminal output for background process";
    default:
        return "Unknown signal";
    }
}

bool isSignalSafe(int signalNumber)
{
    // Signals that are safe to handle in signal handlers
    switch (signalNumber)
    {
    case SIGUSR1:
    case SIGUSR2:
    case SIGALRM:
        return true;
    default:
        return false;
    }
}

bool isAsyncSignalSafe(int signalNumber)
{
    // Signals that are safe for async handling
    return isSignalSafe(signalNumber);
}

bool isProcessAlive(int processId)
{
    if (processId <= 0)
        return false;

    return kill(processId, 0) == 0;
}

std::string getProcessName(int processId)
{
    // Simplified implementation - in real code you'd read /proc/pid/comm
    return "Process-" + std::to_string(processId);
}

#ifdef _WIN32
bool installWindowsSignalHandler(int signalNumber)
{
    // Windows-specific implementation would go here
    return false;
}

bool removeWindowsSignalHandler(int signalNumber)
{
    // Windows-specific implementation would go here
    return false;
}
#else
bool installUnixSignalHandler(int signalNumber)
{
    // Unix-specific implementation would go here
    return true;
}

bool removeUnixSignalHandler(int signalNumber)
{
    // Unix-specific implementation would go here
    return true;
}
#endif

} // namespace SignalUtils

} // namespace FauxDB
