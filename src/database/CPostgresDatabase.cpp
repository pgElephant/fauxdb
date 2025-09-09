/*-------------------------------------------------------------------------
 *
 * CPostgresDatabase.cpp
 *      PostgreSQL database implementation for FauxDB.
 *      Implements PostgreSQL-specific database operations.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#include "CPostgresDatabase.hpp"

#include "CLogger.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace FauxDB
{

/*-------------------------------------------------------------------------
 * Constructor and Destructor
 *-------------------------------------------------------------------------*/
CPostgresDatabase::CPostgresDatabase()
    : libpq_(std::make_unique<CLibpq>()),
      basicAuth_(std::make_unique<CBasicAuth>()), connectionEstablished_(false)
{
    initializePostgresDefaults();
}

CPostgresDatabase::~CPostgresDatabase()
{
    disconnect();
}

bool CPostgresDatabase::initialize(const CDatabaseConfig& config)
{
    try
    {
        postgresConfig_ = config;
        initializePostgresDefaults();
        return true;
    }
    catch (const std::exception& e)
    {
        lastError_ =
            "Exception during initialization: " + std::string(e.what());
        return false;
    }
}

/*-------------------------------------------------------------------------
 * Core database interface implementation
 *-------------------------------------------------------------------------*/
bool CPostgresDatabase::connect(const CDatabaseConfig& config)
{
    if (connectionEstablished_)
    {
        disconnect();
    }

    try
    {
        /* Configure the database connection */
        postgresConfig_.host = config.host;
        postgresConfig_.port = config.port;
        postgresConfig_.database = config.database;
        postgresConfig_.username = config.username;
        postgresConfig_.password = config.password;
        postgresConfig_.options = config.options;

        /* Create PLibpqConfig for CLibpq */
        PLibpqConfig libpqConfig;
        libpqConfig.host = postgresConfig_.host;
        libpqConfig.port = postgresConfig_.port;
        libpqConfig.database = postgresConfig_.database;
        libpqConfig.username = postgresConfig_.username;
        libpqConfig.password = postgresConfig_.password;

        /* Connect using CLibpq */
        if (!libpq_->connect(libpqConfig))
        {
            lastError_ = libpq_->getLastError();
            return false;
        }

        connectionEstablished_ = true;
        return true;
    }
    catch (const std::exception& e)
    {
        lastError_ = "Exception during connection: " + std::string(e.what());
        return false;
    }
}

bool CPostgresDatabase::connect(const CDatabaseConfig& config,
                                const AuthConfig& authConfig)
{
    if (connectionEstablished_)
    {
        disconnect();
    }

    try
    {
        /* Set authentication configuration */
        authConfig_ = authConfig;
        if (!basicAuth_->initialize(authConfig))
        {
            lastError_ = "Failed to initialize authentication: " +
                         basicAuth_->getLastError();
            return false;
        }

        /* Configure the database connection */
        postgresConfig_.host = config.host;
        postgresConfig_.port = config.port;
        postgresConfig_.database = config.database;
        postgresConfig_.username = config.username;
        postgresConfig_.password = config.password;
        postgresConfig_.options = config.options;

        /* Build connection string with authentication */
        std::string connectionString =
            basicAuth_->buildPostgreSQLConnectionString(
                postgresConfig_.host, postgresConfig_.port,
                postgresConfig_.database);

        /* Connect using CLibpq with authentication */
        if (!libpq_->connect(connectionString))
        {
            lastError_ = libpq_->getLastError();
            return false;
        }

        connectionEstablished_ = true;
        return true;
    }
    catch (const std::exception& e)
    {
        lastError_ = "Exception during authenticated connection: " +
                     std::string(e.what());
        return false;
    }
}

void CPostgresDatabase::disconnect()
{
    if (connectionEstablished_)
    {
        if (libpq_->isTransactionActive())
        {
            rollbackTransaction();
        }

        libpq_->disconnect();
        connectionEstablished_ = false;
    }
}

bool CPostgresDatabase::isConnected() const
{
    return libpq_ && libpq_->isConnected();
}

CDatabaseStatus CPostgresDatabase::getStatus() const
{
    if (!libpq_)
    {
        return CDatabaseStatus::DISCONNECTED;
    }

    ConnStatusType status = libpq_->getConnectionStatus();
    switch (status)
    {
    case CONNECTION_OK:
        return CDatabaseStatus::CONNECTED;
    case CONNECTION_BAD:
        return CDatabaseStatus::ERROR;
    case CONNECTION_STARTED:
    case CONNECTION_MADE:
    case CONNECTION_AWAITING_RESPONSE:
    case CONNECTION_AUTH_OK:
    case CONNECTION_SETENV:
    case CONNECTION_SSL_STARTUP:
    case CONNECTION_NEEDED:
    case CONNECTION_CHECK_WRITABLE:
    case CONNECTION_CONSUME:
    case CONNECTION_GSS_STARTUP:
        return CDatabaseStatus::CONNECTING;
    default:
        return CDatabaseStatus::ERROR;
    }
}

CDatabaseQueryResult CPostgresDatabase::executeQuery(const std::string& query)
{
    CDatabaseQueryResult result;

    if (!isConnected())
    {
        result.success = false;
        result.message = "Not connected to database";
        return result;
    }

    try
    {
        auto libpqResult = libpq_->executeQuery(query);

        if (!libpqResult)
        {
            result.success = false;
            result.message = getLastError();
            return result;
        }

        if (libpqResult->isTuplesOk())
        {
            result.success = true;
            result.rowsAffected = libpqResult->getRowCount();
            result.columnNames = libpqResult->getColumnNames();
            result.rows = libpqResult->getAllRows();
        }
        else if (libpqResult->isCommandOk())
        {
            result.success = true;
            result.rowsAffected = 0;
        }
        else
        {
            result.success = false;
            result.message = libpqResult->getErrorMessage();
        }

        return result;
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message =
            "Exception during query execution: " + std::string(e.what());
        return result;
    }
}

CDatabaseQueryResult
CPostgresDatabase::executeQuery(const std::string& query,
                                const std::vector<std::string>& parameters)
{
    CDatabaseQueryResult result;

    if (!isConnected())
    {
        result.success = false;
        result.message = "Not connected to database";
        return result;
    }

    try
    {
        /* Convert string parameters to const char* for PQexecParams */
        std::vector<const char*> paramPtrs;
        for (const auto& param : parameters)
        {
            paramPtrs.push_back(param.c_str());
        }

        auto libpqResult = libpq_->executeQuery(query, parameters);

        if (!libpqResult)
        {
            result.success = false;
            result.message = getLastError();
            return result;
        }

        if (libpqResult->isTuplesOk())
        {
            result.success = true;
            result.rowsAffected = libpqResult->getRowCount();
            result.columnNames = libpqResult->getColumnNames();
            result.rows = libpqResult->getAllRows();
        }
        else if (libpqResult->isCommandOk())
        {
            result.success = true;
            result.rowsAffected = 0;
        }
        else
        {
            result.success = false;
            result.message = libpqResult->getErrorMessage();
        }
        return result;
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Exception during parameterized query execution: " +
                         std::string(e.what());
        return result;
    }
}

CDatabaseQueryResult
CPostgresDatabase::executeQuery(const std::vector<uint8_t>& queryData)
{
    /* Convert binary query data to string and execute */
    std::string query(queryData.begin(), queryData.end());
    return executeQuery(query);
}

CDatabaseQueryResult
CPostgresDatabase::executePreparedQuery(const std::string& queryName,
                                        const std::vector<std::string>& params)
{
    CDatabaseQueryResult result;

    if (!isConnected())
    {
        result.success = false;
        result.message = "Not connected to database";
        return result;
    }

    try
    {
        /* For now, treat prepared queries as regular parameterized queries */
        /* Prepared statement handling */
        auto libpqResult = libpq_->executeQuery(queryName, params);

        if (!libpqResult)
        {
            result.success = false;
            result.message = getLastError();
            return result;
        }

        if (libpqResult->isTuplesOk())
        {
            result.success = true;
            result.rowsAffected = libpqResult->getRowCount();
            result.columnNames = libpqResult->getColumnNames();
            result.rows = libpqResult->getAllRows();
        }
        else if (libpqResult->isCommandOk())
        {
            result.success = true;
            result.rowsAffected = 0;
        }
        else
        {
            result.success = false;
            result.message = libpqResult->getErrorMessage();
        }

        return result;
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Exception during prepared query execution: " +
                         std::string(e.what());
        return result;
    }
}

bool CPostgresDatabase::beginTransaction()
{
    if (!isConnected())
    {
        return false;
    }

    try
    {
        if (libpq_->isTransactionActive())
        {
            return false; /* Already in transaction */
        }

        bool success = libpq_->beginTransaction();

        if (success)
        {
            logDatabaseEvent("TRANSACTION_BEGIN",
                             "Transaction started successfully");
        }
        else
        {
            logDatabaseEvent("ERROR",
                             "Transaction begin failed: " + getLastError());
        }

        return success;
    }
    catch (const std::exception& e)
    {
        logDatabaseEvent("ERROR",
                         "Transaction begin failed: " + std::string(e.what()));
        return false;
    }
}

bool CPostgresDatabase::commitTransaction()
{
    if (!isConnected())
    {
        return false;
    }

    try
    {
        if (!libpq_->isTransactionActive())
        {
            return false; /* No active transaction */
        }

        bool success = libpq_->commitTransaction();

        if (success)
        {
            logDatabaseEvent("TRANSACTION_COMMIT",
                             "Transaction committed successfully");
        }

        return success;
    }
    catch (const std::exception& e)
    {
        logDatabaseEvent("ERROR",
                         "Transaction commit failed: " + std::string(e.what()));
        return false;
    }
}

bool CPostgresDatabase::rollbackTransaction()
{
    if (!isConnected())
    {
        return false;
    }

    try
    {
        if (!libpq_->isTransactionActive())
        {
            return false; /* No active transaction */
        }

        bool success = libpq_->rollbackTransaction();

        if (success)
        {
            logDatabaseEvent("TRANSACTION_ROLLBACK",
                             "Transaction rolled back successfully");
        }

        return success;
    }
    catch (const std::exception& e)
    {
        logDatabaseEvent("ERROR", "Transaction rollback failed: " +
                                      std::string(e.what()));
        return false;
    }
}

CDatabaseTransactionStatus CPostgresDatabase::getTransactionStatus() const
{
    if (!isConnected())
    {
        return CDatabaseTransactionStatus::NO_TRANSACTION;
    }

    if (libpq_->isTransactionActive())
    {
        return CDatabaseTransactionStatus::TRANSACTION_ACTIVE;
    }

    return CDatabaseTransactionStatus::NO_TRANSACTION;
}

/*-------------------------------------------------------------------------
 * Placeholder implementations for remaining pure virtual functions
 * These should be implemented with actual PostgreSQL functionality
 *-------------------------------------------------------------------------*/
bool CPostgresDatabase::createTable(const std::string& tableName,
                                    const std::vector<std::string>& columns)
{
    if (!isConnected())
    {
        return false;
    }

    std::stringstream sql;
    sql << "CREATE TABLE IF NOT EXISTS \"" << tableName << "\" (";

    if (columns.empty())
    {
        /* Default MongoDB-style table with JSONB document storage */
        sql << "_id VARCHAR(24) PRIMARY KEY, "
            << "document JSONB NOT NULL, "
            << "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            << "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP";
    }
    else
    {
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (i > 0)
                sql << ", ";
            sql << "\"" << columns[i] << "\" TEXT";
        }
    }

    sql << ")";

    try
    {
        auto result = executeQuery(sql.str());
        return result.success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool CPostgresDatabase::dropTable(const std::string& tableName)
{
    if (!isConnected())
    {
        return false;
    }

    std::stringstream sql;
    sql << "DROP TABLE IF EXISTS \"" << tableName << "\"";

    try
    {
        auto result = executeQuery(sql.str());
        return result.success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool CPostgresDatabase::alterTable(const std::string& tableName,
                                   const std::string& operation)
{
    if (!isConnected())
    {
        return false;
    }

    std::stringstream sql;
    sql << "ALTER TABLE \"" << tableName << "\" " << operation;

    try
    {
        auto result = executeQuery(sql.str());
        return result.success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::vector<std::string> CPostgresDatabase::getTableNames()
{
    if (!isConnected())
    {
        return {};
    }

    std::vector<std::string> tables;
    std::string sql =
        "SELECT tablename FROM pg_tables WHERE schemaname = 'public'";

    try
    {
        auto result = executeQuery(sql);
        if (result.success)
        {
            for (const auto& row : result.rows)
            {
                if (!row.empty())
                {
                    tables.push_back(row[0]);
                }
            }
        }
    }
    catch (const std::exception&)
    {
        /* Return empty vector on error */
    }

    return tables;
}

std::vector<std::string>
CPostgresDatabase::getColumnNames(const std::string& tableName)
{
    if (!isConnected())
    {
        return {};
    }

    std::vector<std::string> columns;
    std::stringstream sql;
    sql << "SELECT column_name FROM information_schema.columns "
        << "WHERE table_name = '" << tableName << "' "
        << "AND table_schema = 'public' "
        << "ORDER BY ordinal_position";

    try
    {
        auto result = executeQuery(sql.str());
        if (result.success)
        {
            for (const auto& row : result.rows)
            {
                if (!row.empty())
                {
                    columns.push_back(row[0]);
                }
            }
        }
    }
    catch (const std::exception&)
    {
        /* Return empty vector on error */
    }

    return columns;
}

bool CPostgresDatabase::insertData(
    const std::string& tableName, const std::vector<std::string>& columns,
    const std::vector<std::vector<std::string>>& values)
{
    if (!isConnected() || columns.empty() || values.empty())
    {
        return false;
    }

    std::stringstream sql;
    sql << "INSERT INTO \"" << tableName << "\" (";

    for (size_t i = 0; i < columns.size(); ++i)
    {
        if (i > 0)
            sql << ", ";
        sql << "\"" << columns[i] << "\"";
    }

    sql << ") VALUES ";

    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
            sql << ", ";
        sql << "(";

        for (size_t j = 0; j < values[i].size() && j < columns.size(); ++j)
        {
            if (j > 0)
                sql << ", ";
            sql << "'" << values[i][j] << "'";
        }

        sql << ")";
    }

    try
    {
        auto result = executeQuery(sql.str());
        return result.success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool CPostgresDatabase::updateData(const std::string& tableName,
                                   const std::vector<std::string>& setColumns,
                                   const std::vector<std::string>& setValues,
                                   const std::string& whereClause)
{
    if (!isConnected() || setColumns.empty() || setValues.empty() ||
        setColumns.size() != setValues.size())
    {
        return false;
    }

    std::stringstream sql;
    sql << "UPDATE \"" << tableName << "\" SET ";

    for (size_t i = 0; i < setColumns.size(); ++i)
    {
        if (i > 0)
            sql << ", ";
        sql << "\"" << setColumns[i] << "\" = '" << setValues[i] << "'";
    }

    if (!whereClause.empty())
    {
        sql << " WHERE " << whereClause;
    }

    try
    {
        auto result = executeQuery(sql.str());
        return result.success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool CPostgresDatabase::deleteData(const std::string& tableName,
                                   const std::string& whereClause)
{
    if (!isConnected())
    {
        return false;
    }

    std::stringstream sql;
    sql << "DELETE FROM \"" << tableName << "\"";

    if (!whereClause.empty())
    {
        sql << " WHERE " << whereClause;
    }

    try
    {
        auto result = executeQuery(sql.str());
        return result.success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool CPostgresDatabase::createIndex(const std::string& tableName,
                                    const std::string& indexName,
                                    const std::vector<std::string>& columns)
{
    if (!isConnected() || columns.empty())
    {
        return false;
    }

    std::stringstream sql;
    sql << "CREATE INDEX IF NOT EXISTS \"" << indexName << "\" ON \""
        << tableName << "\" (";

    for (size_t i = 0; i < columns.size(); ++i)
    {
        if (i > 0)
            sql << ", ";
        sql << "\"" << columns[i] << "\"";
    }

    sql << ")";

    try
    {
        auto result = executeQuery(sql.str());
        return result.success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool CPostgresDatabase::dropIndex(const std::string& indexName)
{
    if (!isConnected())
    {
        return false;
    }

    std::stringstream sql;
    sql << "DROP INDEX IF EXISTS \"" << indexName << "\"";

    try
    {
        auto result = executeQuery(sql.str());
        return result.success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool CPostgresDatabase::vacuumDatabase()
{
    /* Vacuum operation */
    return false;
}

bool CPostgresDatabase::analyzeDatabase()
{
    /* Analyze operation */
    return false;
}

void CPostgresDatabase::setConfig(const CDatabaseConfig& config)
{
    postgresConfig_ = config;
}

CDatabaseConfig CPostgresDatabase::getConfig() const
{
    /* Config retrieval */
    return CDatabaseConfig{};
}

void CPostgresDatabase::setConnectionTimeout(std::chrono::milliseconds timeout)
{
    /* Timeout setting */
}

void CPostgresDatabase::setQueryTimeout(std::chrono::milliseconds timeout)
{
    /* Query timeout setting */
}

std::string CPostgresDatabase::getDatabaseInfo() const
{
    /* Database info retrieval */
    return "PostgreSQL Database";
}

std::string CPostgresDatabase::getVersion() const
{
    /* Version retrieval */
    return "Unknown";
}

size_t CPostgresDatabase::getActiveConnections() const
{
    /* Connection counting */
    return 1;
}

bool CPostgresDatabase::healthCheck()
{
    /* Health check */
    return isConnected();
}

bool CPostgresDatabase::backupDatabase(const std::string& backupPath)
{
    /* Backup functionality */
    return false;
}

bool CPostgresDatabase::restoreDatabase(const std::string& backupPath)
{
    /* Restore functionality */
    return false;
}

bool CPostgresDatabase::exportData(const std::string& tableName,
                                   const std::string& exportPath)
{
    /* Export functionality */
    return false;
}

bool CPostgresDatabase::importData(const std::string& tableName,
                                   const std::string& importPath)
{
    /* Import functionality */
    return false;
}

bool CPostgresDatabase::createUser(const std::string& username,
                                   const std::string& password)
{
    /* User creation */
    return false;
}

bool CPostgresDatabase::dropUser(const std::string& username)
{
    /* User dropping */
    return false;
}

bool CPostgresDatabase::grantPrivileges(const std::string& username,
                                        const std::string& privileges)
{
    /* Privilege granting */
    return false;
}

bool CPostgresDatabase::revokePrivileges(const std::string& username,
                                         const std::string& privileges)
{
    /* Privilege revocation */
    return false;
}

std::string CPostgresDatabase::getLastError() const
{
    if (libpq_)
    {
        return libpq_->getLastError();
    }
    return lastError_;
}

void CPostgresDatabase::clearErrors()
{
    lastError_.clear();
}

bool CPostgresDatabase::hasErrors() const
{
    return !lastError_.empty();
}

void CPostgresDatabase::setConnectionCallback(
    std::function<void(CDatabaseStatus)> callback)
{
    connectionCallback_ = callback;
}

void CPostgresDatabase::setQueryCallback(
    std::function<void(const std::string&, const CDatabaseQueryResult&)>
        callback)
{
    queryCallback_ = callback;
}

void CPostgresDatabase::setErrorCallback(
    std::function<void(const std::string&, const std::string&)> callback)
{
    errorCallback_ = callback;
}

/*-------------------------------------------------------------------------
 * Protected method implementations
 *-------------------------------------------------------------------------*/
void CPostgresDatabase::setStatus(CDatabaseStatus status)
{
    CDatabase::setStatus(status);
}

void CPostgresDatabase::setTransactionStatus(CDatabaseTransactionStatus status)
{
    CDatabase::setTransactionStatus(status);
}

void CPostgresDatabase::updateLastActivity()
{
    CDatabase::updateLastActivity();
}

void CPostgresDatabase::logDatabaseEvent(const std::string& event,
                                         const std::string& details)
{
    CDatabase::logDatabaseEvent(event, details);
}

CDatabaseQueryResult CPostgresDatabase::processQuery(const std::string& query)
{
    return CDatabase::processQuery(query);
}

CDatabaseQueryResult CPostgresDatabase::processParameterizedQuery(
    const std::string& query, const std::vector<std::string>& parameters)
{
    return CDatabase::processParameterizedQuery(query, parameters);
}

bool CPostgresDatabase::validateQuery(const std::string& query)
{
    return CDatabase::validateQuery(query);
}

std::string CPostgresDatabase::sanitizeQuery(const std::string& query)
{
    return CDatabase::sanitizeQuery(query);
}

bool CPostgresDatabase::validateTransactionState()
{
    return CDatabase::validateTransactionState();
}

void CPostgresDatabase::logTransactionEvent(const std::string& event)
{
    CDatabase::logTransactionEvent(event);
}

/*-------------------------------------------------------------------------
 * Private method implementations
 *-------------------------------------------------------------------------*/
void CPostgresDatabase::initializePostgresDefaults()
{
    postgresConfig_.host = "localhost";
    postgresConfig_.port = "5432";
    postgresConfig_.database = "fauxdb";
    postgresConfig_.username = "postgres";
    postgresConfig_.password = "";
    postgresConfig_.sslmode = "prefer";
    postgresConfig_.applicationName = "FauxDB";
    postgresConfig_.clientEncoding = "UTF8";
    postgresConfig_.timezone = "UTC";
    postgresConfig_.binaryResults = false;
    postgresConfig_.preparedStatements = true;
}

/* Connection string building now handled by CLibpq */

void CPostgresDatabase::setPostgresError()
{
    if (libpq_)
    {
        lastError_ = libpq_->getLastError();
    }
    else
    {
        lastError_ = "PostgreSQL connection not available";
    }
}

/* Transaction status now handled by CLibpq */

bool CPostgresDatabase::connect()
{
    if (!libpq_)
    {
        lastError_ = "LibPQ not initialized";
        return false;
    }

    try
    {
        if (libpq_->connect(config_.host, config_.port, config_.database,
                            config_.username, config_.password))
        {
            setStatus(CDatabaseStatus::CONNECTED);
            updateLastActivity();
            return true;
        }
        else
        {
            lastError_ = "Failed to connect to database";
            setStatus(CDatabaseStatus::ERROR);
            return false;
        }
    }
    catch (const std::exception& e)
    {
        lastError_ = "Connection error: " + std::string(e.what());
        setStatus(CDatabaseStatus::ERROR);
        return false;
    }
}

bool CPostgresDatabase::ping()
{
    // Always attempt a real query, even if isConnected() returns true
    try
    {
        auto result = executeQuery("SELECT 1");
        if (!result.success)
        {
            disconnect();
            lastError_ = "Ping failed: could not execute query.";
            return false;
        }
        return true;
    }
    catch (const std::exception& e)
    {
        disconnect();
        lastError_ = "Ping failed: " + std::string(e.what());
        return false;
    }
}

size_t CPostgresDatabase::getAffectedRows() const
{
    return affectedRows_;
}

size_t CPostgresDatabase::getLastInsertId() const
{
    return lastInsertId_;
}

std::string CPostgresDatabase::getServerVersion() const
{
    if (!isConnected())
    {
        return "";
    }

    try
    {
        // Note: This would need to be const-correct, but for now we'll use a
        // workaround In a real implementation, you'd need to make executeQuery
        // const or have a separate const method
        return "PostgreSQL (version query not implemented)";
    }
    catch (const std::exception& e)
    {
        // Note: lastError_ is const in const methods, so we can't set it here
        // In a real implementation, you'd need a different approach for error
        // handling in const methods
    }

    return "";
}

std::string CPostgresDatabase::getConnectionInfo() const
{
    std::stringstream ss;
    ss << "Host: " << config_.host << ", Port: " << config_.port
       << ", Database: " << config_.database << ", User: " << config_.username;
    return ss.str();
}

void CPostgresDatabase::setAuthConfig(const AuthConfig& authConfig)
{
    authConfig_ = authConfig;
    if (basicAuth_)
    {
        basicAuth_->initialize(authConfig);
    }
}

AuthConfig CPostgresDatabase::getAuthConfig() const
{
    return authConfig_;
}

bool CPostgresDatabase::authenticate(const std::string& username,
                                     const std::string& password)
{
    if (!basicAuth_)
    {
        lastError_ = "Basic authentication not initialized";
        return false;
    }

    return basicAuth_->authenticate(username, password);
}

bool CPostgresDatabase::isAuthenticationRequired() const
{
    if (!basicAuth_)
    {
        return false;
    }

    return basicAuth_->isRequired();
}

} /* namespace FauxDB */
