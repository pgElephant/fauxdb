/*-------------------------------------------------------------------------
 *
 * CDatabase.hpp
 *      Abstract base class for database operations in FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace FauxDB
{

enum class CDatabaseStatus : uint8_t
{
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ERROR = 3
};

struct CDatabaseQueryResult
{
    bool success;
    std::string message;
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> columnNames;
    std::vector<std::string> columnTypes;
    size_t rowsAffected;
    std::chrono::milliseconds executionTime;

    CDatabaseQueryResult() : success(false), rowsAffected(0), executionTime(0)
    {
    }
};

enum class CDatabaseTransactionStatus : uint8_t
{
    NO_TRANSACTION = 0,
    TRANSACTION_ACTIVE = 1,
    TRANSACTION_COMMITTED = 2,
    TRANSACTION_ROLLED_BACK = 3
};

struct CDatabaseConfig
{
    std::string host;
    std::string port;
    std::string database;
    std::string username;
    std::string password;
    std::string options;
    std::string sslmode;
    std::string applicationName;
    std::string clientEncoding;
    std::string timezone;
    bool binaryResults;
    bool preparedStatements;
    std::chrono::milliseconds connectionTimeout;
    std::chrono::milliseconds queryTimeout;
    size_t maxConnections;
    bool autoCommit;
    bool sslEnabled;

    CDatabaseConfig()
        : host("localhost"), port("5432"), database(""), username(""),
          password(""), options(""), sslmode("prefer"),
          applicationName("FauxDB"), clientEncoding("UTF8"), timezone("UTC"),
          binaryResults(false), preparedStatements(true),
          connectionTimeout(5000), queryTimeout(30000), maxConnections(10),
          autoCommit(true), sslEnabled(false)
    {
    }
};

class CDatabase
{
  public:
    CDatabase();
    virtual ~CDatabase();

    virtual bool initialize(const CDatabaseConfig& config) = 0;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual CDatabaseStatus getStatus() const = 0;

    virtual CDatabaseQueryResult executeQuery(const std::string& query) = 0;
    virtual CDatabaseQueryResult
    executePreparedQuery(const std::string& queryName,
                         const std::vector<std::string>& params) = 0;

    virtual bool beginTransaction() = 0;
    virtual bool commitTransaction() = 0;
    virtual bool rollbackTransaction() = 0;
    virtual CDatabaseTransactionStatus getTransactionStatus() const = 0;

    virtual std::string getLastError() const = 0;
    virtual size_t getLastInsertId() const = 0;
    virtual size_t getAffectedRows() const = 0;

    virtual bool ping() = 0;
    virtual std::string getServerVersion() const = 0;
    virtual std::string getConnectionInfo() const = 0;

  protected:
    CDatabaseConfig config_;
    CDatabaseStatus status_;
    CDatabaseTransactionStatus transactionStatus_;
    std::string lastError_;
    size_t lastInsertId_;
    size_t affectedRows_;
    bool connected_;
    std::chrono::steady_clock::time_point lastActivity_;
    std::chrono::steady_clock::time_point lastErrorTime_;
    std::function<void(const std::string&, const std::string&)> errorCallback_;

    void setStatus(CDatabaseStatus status);
    void setTransactionStatus(CDatabaseTransactionStatus status);
    void updateLastActivity();
    void logDatabaseEvent(const std::string& event, const std::string& details);
    CDatabaseQueryResult processQuery(const std::string& query);
    CDatabaseQueryResult
    processParameterizedQuery(const std::string& query,
                              const std::vector<std::string>& params);
    bool validateQuery(const std::string& query);
    bool validateTransactionState();
    void logTransactionEvent(const std::string& event);
    std::string sanitizeQuery(const std::string& query);
    void initializeDefaults();
    void cleanupState();
    std::string buildErrorMessage(const std::string& operation,
                                  const std::string& details);
    bool isTransactionAllowed() const;

  private:
    CDatabase(const CDatabase&) = delete;
    CDatabase& operator=(const CDatabase&) = delete;
};

} // namespace FauxDB
