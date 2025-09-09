/*-------------------------------------------------------------------------
 *
 * CSignal.hpp
 *      Signal handling class for FauxDB.
 *      Provides cross-platform signal handling and management.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CLogger.hpp"

#include <atomic>
#include <csignal>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace FauxDB
{

enum class CSignalType : int
{
    Interrupt = SIGINT,          // Ctrl+C
    Terminate = SIGTERM,         // Termination request
    Quit = SIGQUIT,              // Quit request
    Abort = SIGABRT,             // Abort signal
    SegmentationFault = SIGSEGV, // Segmentation fault
    FloatingPoint = SIGFPE,      // Floating point exception
    IllegalInstruction = SIGILL, // Illegal instruction
    BusError = SIGBUS,           // Bus error
    User1 = SIGUSR1,             // User defined signal 1
    User2 = SIGUSR2,             // User defined signal 2
    Pipe = SIGPIPE,              // Broken pipe
    Alarm = SIGALRM,             // Timer signal
    Child = SIGCHLD,             // Child stopped or terminated
    Continue = SIGCONT,          // Continue if stopped
    Stop = SIGSTOP,              // Stop process
    TerminalStop = SIGTSTP,      // Stop typed at terminal
    TerminalInput = SIGTTIN,     // Terminal input for background process
    TerminalOutput = SIGTTOU     // Terminal output for background process
};

using CSignalHandler = std::function<void(CSignalType, int)>;

struct CSignalConfig
{
    bool enableDefaultHandlers;
    bool enableAsyncHandling;
    bool enableSignalMasking;
    bool enableSignalQueuing;
    std::string logFile;

    CSignalConfig()
        : enableDefaultHandlers(true), enableAsyncHandling(false),
          enableSignalMasking(false), enableSignalQueuing(false), logFile("")
    {
    }
};

struct CSignalInfo
{
    CSignalType type;
    int signalNumber;
    std::string name;
    std::string description;
    bool isFatal;
    bool isIgnorable;

    CSignalInfo()
        : type(CSignalType::Interrupt), signalNumber(0), isFatal(false),
          isIgnorable(true)
    {
    }
};

class CSignal
{
  public:
    CSignal();
    virtual ~CSignal();

    /* Core signal interface */
    virtual bool initialize(const CSignalConfig& config = CSignalConfig());
    virtual void shutdown();
    virtual bool isInitialized() const;

    /* Signal handler management */
    virtual bool registerHandler(CSignalType signalType,
                                 CSignalHandler handler);
    virtual bool unregisterHandler(CSignalType signalType);
    virtual bool hasHandler(CSignalType signalType) const;
    virtual CSignalHandler getHandler(CSignalType signalType) const;

    /* Signal control */
    virtual bool blockSignal(CSignalType signalType);
    virtual bool unblockSignal(CSignalType signalType);
    virtual bool isSignalBlocked(CSignalType signalType) const;
    virtual bool ignoreSignal(CSignalType signalType);
    virtual bool resetSignal(CSignalType signalType);

    /* Signal state queries */
    virtual bool shouldExit() const;
    virtual bool shouldReload() const;
    virtual void clearReloadFlag();

    /* Signal sending */
    virtual bool sendSignal(int processId, CSignalType signalType);
    virtual bool sendSignalToSelf(CSignalType signalType);
    virtual bool raiseSignal(CSignalType signalType);

    /* Signal information */
    virtual CSignalInfo getSignalInfo(CSignalType signalType) const;
    virtual std::string getSignalName(CSignalType signalType) const;
    virtual std::string getSignalDescription(CSignalType signalType) const;
    virtual bool isSignalFatal(CSignalType signalType) const;
    virtual bool isSignalIgnorable(CSignalType signalType) const;

    /* Signal monitoring */
    virtual bool enableSignalMonitoring(bool enabled);
    virtual bool isSignalMonitoringEnabled() const;
    virtual std::vector<CSignalInfo> getActiveSignals() const;
    virtual std::vector<CSignalInfo> getBlockedSignals() const;

    /* Signal statistics */
    virtual size_t getSignalCount(CSignalType signalType) const;
    virtual void resetSignalCount(CSignalType signalType);
    virtual void resetAllSignalCounts();
    virtual std::string getSignalStatistics() const;

    /* Signal configuration */
    virtual void setConfig(const CSignalConfig& config);
    virtual CSignalConfig getConfig() const;
    virtual bool validateConfig(const CSignalConfig& config) const;

    /* Error handling */
    virtual std::string getLastError() const;
    virtual void clearErrors();
    virtual bool hasErrors() const;

    /* Logger management */
    virtual void setLogger(std::shared_ptr<CLogger> logger);

  protected:
    /* Signal state */
    CSignalConfig config_;
    bool initialized_;
    bool monitoringEnabled_;

    /* Signal handlers */
    std::unordered_map<CSignalType, CSignalHandler> handlers_;
    std::unordered_map<CSignalType, CSignalHandler> defaultHandlers_;

    /* Signal blocking */
    std::unordered_map<CSignalType, bool> blockedSignals_;

    /* Signal statistics */
    std::unordered_map<CSignalType, size_t> signalCounts_;

    /* Error state */
    mutable std::string lastError_;

    /* Logger */
    std::shared_ptr<CLogger> logger_;

    /* Protected utility methods */
    virtual void setError(const std::string& error) const;
    virtual bool validateSignalType(CSignalType signalType) const;
    virtual bool installSystemHandler(CSignalType signalType);
    virtual bool restoreSystemHandler(CSignalType signalType);

    virtual void handleSystemSignal(int signalNumber);
    virtual void logSignal(CSignalType signalType, const std::string& action);
    virtual void updateSignalCount(CSignalType signalType);

    /* Static signal handler for system calls */
    static void staticSignalHandler(int signalNumber);

  private:
    /* Private utility methods */
    void initializeDefaults();
    void cleanupState();

    /* Signal handler helpers */
    bool createDefaultHandler(CSignalType signalType);
    bool removeDefaultHandler(CSignalType signalType);
    CSignalHandler findHandler(CSignalType signalType) const;

    /* Signal control helpers */
    bool setSignalAction(CSignalType signalType, void (*action)(int));
    bool getSignalAction(CSignalType signalType, struct sigaction* action);
    bool setSignalMask(const std::vector<CSignalType>& signals, bool block);

    /* Utility methods */
    std::string buildErrorMessage(const std::string& operation,
                                  const std::string& details) const;
    void resetSignalState();
    bool isValidProcessId(int processId) const;
    std::string getSignalString(CSignalType signalType) const;

    /* Global signal handler management */
    static std::unordered_map<int, CSignal*> g_signalInstances;
    static std::mutex g_signalMutex;

    static void registerGlobalInstance(int signalNumber, CSignal* instance);
    static void unregisterGlobalInstance(int signalNumber);
    static CSignal* findGlobalInstance(int signalNumber);
};

namespace SignalUtils
{

/* Signal name utilities */
std::string getSignalName(int signalNumber);
std::string getSignalDescription(int signalNumber);

/* Signal safety utilities */
bool isSignalSafe(int signalNumber);
bool isAsyncSignalSafe(int signalNumber);

/* Process utilities */
bool isProcessAlive(int processId);
std::string getProcessName(int processId);

/* Platform-specific utilities */
#ifdef _WIN32
bool installWindowsSignalHandler(int signalNumber);
bool removeWindowsSignalHandler(int signalNumber);
#else
bool installUnixSignalHandler(int signalNumber);
bool removeUnixSignalHandler(int signalNumber);
#endif
} // namespace SignalUtils

} /* namespace FauxDB */
