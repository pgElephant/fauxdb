/*-------------------------------------------------------------------------
 *
 * CLogger.hpp
 *      Logging system implementation for FauxDB.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once
#include "CServerConfig.hpp"
#include "IInterfaces.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace FauxDB
{

class CLogger : public ILogger
{
  public:
    explicit CLogger(const CServerConfig& config);
    virtual ~CLogger();
    void log(CLogLevel level, const std::string& message) override;
    void setLogLevel(CLogLevel level) override;
    CLogLevel getLogLevel() const noexcept override;
    std::error_code initialize() override;
    void shutdown() noexcept override;
    void logWithContext(CLogLevel level, const std::string& message,
                        const std::string& context);
    void logWithTimestamp(CLogLevel level, const std::string& message);
    void logWithMetadata(
        CLogLevel level, const std::string& message,
        const std::unordered_map<std::string, std::string>& metadata);
    void setLogFile(const std::string& filename);
    void setErrorLogFile(const std::string& filename);
    void enableConsoleOutput(bool enable);
    void enableFileOutput(bool enable);
    void setMaxLogFileSize(size_t maxSize);
    void setLogRotationCount(size_t count);
    void setTimestampFormat(const std::string& format);
    void rotateLogFiles();
    void cleanupOldLogFiles();

  private:
    CServerConfig config_;
    std::string logFile_;
    std::string errorLogFile_;
    bool consoleOutput_;
    bool fileOutput_;
    size_t maxLogFileSize_;
    size_t logRotationCount_;
    std::string timestampFormat_;
    std::atomic<CLogLevel> logLevel_;
    std::unique_ptr<std::ofstream> fileStream_;
    std::unique_ptr<std::ofstream> errorStream_;
    std::mutex logMutex_;
    std::atomic<bool> initialized_;
    std::queue<std::pair<CLogLevel, std::string>> logQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::thread asyncLoggerThread_;
    std::atomic<bool> stopAsyncLogger_;
    void writeToConsole(const std::string& message);
    void writeToFile(CLogLevel level, const std::string& message);
    std::string formatMessage(CLogLevel level, const std::string& message);
    std::string getLevelString(CLogLevel level) const;
    std::string getTimestamp() const;
    bool shouldLog(CLogLevel level) const noexcept;
    void asyncLoggerFunction();
    void processLogQueue();
    std::error_code openLogFiles();
    void closeLogFiles();
    bool shouldRotateLogFile() const;
    std::string generateRotatedFileName(const std::string& baseName,
                                        size_t index);
};

class CLogFormatter
{
  public:
    CLogFormatter();
    ~CLogFormatter() = default;
    std::string formatAsText(CLogLevel level, const std::string& message,
                             const std::string& timestamp);
    std::string formatAsJSON(CLogLevel level, const std::string& message,
                             const std::string& timestamp);
    std::string formatAsYaml(CLogLevel level, const std::string& message,
                             const std::string& timestamp);
    std::string formatAsToml(CLogLevel level, const std::string& message,
                             const std::string& timestamp);
    std::string formatAsXml(CLogLevel level, const std::string& message,
                            const std::string& timestamp);
    std::string formatAsCsv(CLogLevel level, const std::string& message,
                            const std::string& timestamp);
    std::string formatAsProtobuf(CLogLevel level, const std::string& message,
                                 const std::string& timestamp);
    std::string formatAsFlatbuffers(CLogLevel level, const std::string& message,
                                    const std::string& timestamp);
    void setFormat(const std::string& format);
    void setIncludeTimestamp(bool include);
    void setIncludeLogLevel(bool include);
    void setIncludeContext(bool include);
    void setIndentSize(int size);

  private:
    std::string format_;
    bool includeTimestamp_;
    bool includeLogLevel_;
    bool includeContext_;
    int indentSize_;
    std::string escapeString(const std::string& str);
    std::string formatLogLevel(CLogLevel level);
    std::string formatTimestamp(const std::string& timestamp);
};

class CLogFilter
{
  public:
    CLogFilter();
    ~CLogFilter() = default;
    bool shouldLogMessage(CLogLevel level, const std::string& message,
                          const std::string& context);
    void addLevelFilter(CLogLevel level, bool allow);
    void addContextFilter(const std::string& context, bool allow);
    void addMessageFilter(const std::string& pattern, bool allow);
    void addRegexFilter(const std::string& regex, bool allow);
    void setDefaultAction(bool allow);
    void clearFilters();

  private:
    bool defaultAction_;
    std::unordered_map<CLogLevel, bool> levelFilters_;
    std::unordered_map<std::string, bool> contextFilters_;
    std::vector<std::pair<std::string, bool>> messageFilters_;
    std::vector<std::pair<std::string, bool>> regexFilters_;
    bool matchesPattern(const std::string& text, const std::string& pattern);
    bool matchesRegex(const std::string& text, const std::string& regex);
};

class CLogStats
{
  public:
    CLogStats();
    ~CLogStats() = default;
    void recordLogMessage(CLogLevel level);
    void recordLogFileWrite(size_t bytes);
    void recordLogRotation();
    uint64_t getTotalMessages() const noexcept;
    uint64_t getMessagesByLevel(CLogLevel level) const noexcept;
    uint64_t getTotalBytesWritten() const noexcept;
    uint64_t getTotalRotations() const noexcept;
    std::chrono::system_clock::time_point getStartTime() const noexcept;
    void reset();
    std::string toString() const;

  private:
    std::atomic<uint64_t> totalMessages_{0};
    std::atomic<uint64_t> totalBytesWritten_{0};
    std::atomic<uint64_t> totalRotations_{0};
    std::unordered_map<CLogLevel, std::atomic<uint64_t>> messagesByLevel_;
    std::chrono::system_clock::time_point startTime_;
    mutable std::mutex statsMutex_;
    std::unordered_map<std::string, double> performanceMetrics_;
    std::vector<std::chrono::steady_clock::time_point> accessTimes_;
    size_t maxAccessHistory_;
};

class CLoggerFactory
{
  public:
    enum class LoggerType
    {
        File,
        Console,
        Syslog,
        Network,
        Custom,
        Structured,
        Cloud,
        Distributed,
        Metrics,
        Audit,
        Security,
        Performance,
        Telemetry
    };
    static std::unique_ptr<ILogger> createLogger(LoggerType type,
                                                 const CServerConfig& config);
    static std::string getLoggerTypeName(LoggerType type);
    static LoggerType getLoggerTypeFromString(const std::string& typeName);
};

} /* namespace FauxDB */
