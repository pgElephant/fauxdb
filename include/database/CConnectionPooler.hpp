
#pragma once

#include "CLogger.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace FauxDB
{

using std::function;
using std::shared_ptr;
using std::size_t;
using std::string;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

enum class CConnectionPoolStatus : uint8_t
{
    Available = 0,
    InUse = 1,
    Broken = 2,
    Connecting = 3,
    Disconnected = 4,
    Maintenance = 5
};

struct CConnectionPoolConfig
{
    size_t minConnections;
    size_t maxConnections;
    size_t initialConnections;
    milliseconds connectionTimeout;
    milliseconds idleTimeout;
    milliseconds maxLifetime;
    bool autoReconnect;
    bool validateConnections;
    size_t validationInterval;
    CConnectionPoolConfig()
        : minConnections(5), maxConnections(20), initialConnections(5),
          connectionTimeout(5000), idleTimeout(300000), maxLifetime(3600000),
          autoReconnect(true), validateConnections(true),
          validationInterval(30000)
    {
    }
};

struct CConnectionPoolStats
{
    size_t totalConnections;
    size_t availableConnections;
    size_t inUseConnections;
    size_t brokenConnections;
    size_t totalRequests;
    size_t successfulRequests;
    size_t failedRequests;
    milliseconds averageResponseTime;
    steady_clock::time_point lastReset;
    CConnectionPoolStats()
        : totalConnections(0), availableConnections(0), inUseConnections(0),
          brokenConnections(0), totalRequests(0), successfulRequests(0),
          failedRequests(0), averageResponseTime(0)
    {
    }
};

class CConnectionPooler
{
  public:
    CConnectionPooler();
    virtual ~CConnectionPooler();
    virtual shared_ptr<void> getConnection() = 0;
    virtual void releaseConnection(shared_ptr<void> connection) = 0;
    virtual bool returnConnection(shared_ptr<void> connection) = 0;
    virtual bool initialize(const CConnectionPoolConfig& config) = 0;

    virtual size_t getAvailableConnections() const = 0;
    virtual size_t getInUseConnections() const = 0;
    virtual void setConfig(const CConnectionPoolConfig& config) = 0;
    virtual CConnectionPoolConfig getConfig() const = 0;
    virtual void setConnectionTimeout(milliseconds timeout) = 0;
    virtual void setMaxConnections(size_t maxConnections) = 0;
    virtual void setMinConnections(size_t minConnections) = 0;
    virtual CConnectionPoolStats getStats() const = 0;
    virtual void resetStats() = 0;
    virtual bool healthCheck() = 0;
    virtual string getStatusReport() const = 0;
    virtual bool validateConnection(shared_ptr<void> connection) = 0;
    virtual void
    setConnectionValidator(function<bool(shared_ptr<void>)> validator) = 0;
    virtual void setConnectionFactory(function<shared_ptr<void>()> factory) = 0;
    virtual void setConnectionAcquiredCallback(
        function<void(shared_ptr<void>)> callback) = 0;
    virtual void setConnectionReleasedCallback(
        function<void(shared_ptr<void>)> callback) = 0;
    virtual void setConnectionFailedCallback(
        function<void(shared_ptr<void>, const string&)> callback) = 0;
    virtual void setLogger(std::shared_ptr<CLogger> logger);

  protected:
    CConnectionPoolConfig config_;
    CConnectionPoolStats stats_;
    bool isRunning_;
    steady_clock::time_point startTime_;
    std::shared_ptr<CLogger> logger_;
    function<bool(shared_ptr<void>)> connectionValidator_;
    function<shared_ptr<void>()> connectionFactory_;
    function<void(shared_ptr<void>)> connectionAcquiredCallback_;
    function<void(shared_ptr<void>)> connectionReleasedCallback_;
    function<void(shared_ptr<void>, const string&)> connectionFailedCallback_;
    virtual void updateStats(bool success, milliseconds responseTime);
    virtual bool shouldCreateConnection() const;
    virtual bool shouldRemoveConnection() const;
    virtual void logConnectionEvent(const string& event, const string& details);
    virtual void performMaintenance();
    virtual void cleanupBrokenConnections();
    virtual void validateConnections();
    virtual void adjustPoolSize();

  private:
    steady_clock::time_point lastMaintenance_;
    steady_clock::time_point lastValidation_;
    void initializeDefaults();
    void calculateAverageResponseTime(milliseconds responseTime);
    bool isTimeForMaintenance() const;
    bool isTimeForValidation() const;
};

} // namespace FauxDB
