

/*-------------------------------------------------------------------------
 *
 * CPGConnectionPooler.hpp
 *      PostgreSQL connection pooler for FauxDB.
 *      Manages pooled connections to PostgreSQL database.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CConnectionPooler.hpp"
#include "CPostgresDatabase.hpp"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace FauxDB
{

struct PGConnection
{
    shared_ptr<CPostgresDatabase> database;
    bool inUse;
    steady_clock::time_point lastUsed;
    steady_clock::time_point created;
    PGConnection(std::shared_ptr<CPostgresDatabase> db)
        : database(db), inUse(false),
          lastUsed(std::chrono::steady_clock::now()),
          created(std::chrono::steady_clock::now())
    {
    }
    ~PGConnection() = default;
};

class CPGConnectionPooler : public CConnectionPooler
{
  public:
    CPGConnectionPooler();
    virtual ~CPGConnectionPooler();

    CConnectionPoolStats getStats() const override;
    shared_ptr<void> getConnection() override;
    void releaseConnection(shared_ptr<void> connection) override;
    bool returnConnection(shared_ptr<void> connection) override;

    bool initialize(const CConnectionPoolConfig& config) override;
    bool start();
    void stop();
    bool isRunning() const;
    void shutdown();

    bool addConnection();
    bool removeConnection(shared_ptr<void> connection);

    void setPostgresConfig(const std::string& host, const std::string& port,
                           const std::string& database,
                           const std::string& username,
                           const std::string& password);

    std::shared_ptr<PGConnection> getPostgresConnection();
    void releasePostgresConnection(std::shared_ptr<PGConnection> connection);

    std::size_t getActiveConnections() const;
    std::size_t getIdleConnections() const;
    std::size_t getTotalConnections() const;

    void clearPool();
    std::size_t getPoolSize() const;
    std::size_t getAvailableConnections() const override;
    std::size_t getInUseConnections() const override;
    void setConfig(const CConnectionPoolConfig& config) override;
    CConnectionPoolConfig getConfig() const override;
    void setConnectionTimeout(std::chrono::milliseconds timeout) override;

  private:
    milliseconds connectionTimeout_ = milliseconds(30000);
    size_t maxConnections_ = 1;
    mutable std::mutex statsMutex_;
    size_t totalConnectionsCreated_ = 0;
    size_t totalConnectionsUsed_ = 0;
    size_t totalConnectionsFailed_ = 0;
    steady_clock::time_point lastMaintenanceTime_ = steady_clock::now();

    void setMaxConnections(size_t maxConnections) override;
    void setMinConnections(size_t minConnections) override;
    void resetStats() override;
    bool healthCheck() override;
    string getStatusReport() const override;
    void
    setConnectionValidator(function<bool(shared_ptr<void>)> validator) override;
    void setConnectionFactory(function<shared_ptr<void>()> factory) override;
    void setConnectionAcquiredCallback(
        function<void(shared_ptr<void>)> callback) override;
    void setConnectionReleasedCallback(
        function<void(shared_ptr<void>)> callback) override;
    void setConnectionFailedCallback(
        function<void(shared_ptr<void>, const string&)> callback) override;

  protected:
    void logConnectionEvent(const string& event,
                            const string& details) override;
    void performMaintenance() override;
    void cleanupBrokenConnections() override;
    void validateConnections() override;
    void adjustPoolSize() override;

    std::string host_;
    std::string port_;
    std::string databaseName_;
    std::string username_;
    std::string password_;

    std::vector<std::shared_ptr<PGConnection>> connections_;
    std::vector<std::shared_ptr<PGConnection>> availableConnections_;
    std::vector<std::shared_ptr<PGConnection>> inUseConnections_;

    mutable std::mutex poolMutex_;
    std::condition_variable connectionAvailable_;

    CConnectionPoolConfig config_;
    function<bool(shared_ptr<void>)> connectionValidator_;
    function<shared_ptr<void>()> connectionFactory_;
    function<void(shared_ptr<void>)> connectionAcquiredCallback_;
    function<void(shared_ptr<void>)> connectionReleasedCallback_;
    function<void(shared_ptr<void>, const string&)> connectionFailedCallback_;

    shared_ptr<PGConnection> createNewConnection();
    bool validateConnection(shared_ptr<void> connection) override;
    void closeConnection(shared_ptr<PGConnection> connection);
    string buildConnectionString() const;

    void markConnectionInUse(shared_ptr<PGConnection> connection);
    void markConnectionAvailable(shared_ptr<PGConnection> connection);
    void removeBrokenConnection(shared_ptr<PGConnection> connection);

    void cleanupExpiredConnections();
    void validateConnectionHealth();
    void logPoolStatistics();
};

} // namespace FauxDB
