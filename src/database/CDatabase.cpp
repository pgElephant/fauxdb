/*-------------------------------------------------------------------------
 *
 * CDatabase.cpp
 *      Implementation of database interface for FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#include "CDatabase.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace FauxDB
{

/*-------------------------------------------------------------------------
 * CDatabase implementation
 *-------------------------------------------------------------------------*/
CDatabase::CDatabase()
    : status_(CDatabaseStatus::DISCONNECTED),
      transactionStatus_(CDatabaseTransactionStatus::NO_TRANSACTION),
      lastActivity_(std::chrono::steady_clock::now())
{
    initializeDefaults();
}

CDatabase::~CDatabase()
{
    cleanupState();
}

void CDatabase::setStatus(CDatabaseStatus status)
{
    status_ = status;
}

void CDatabase::setTransactionStatus(CDatabaseTransactionStatus status)
{
    transactionStatus_ = status;
}

void CDatabase::updateLastActivity()
{
    lastActivity_ = std::chrono::steady_clock::now();
}

void CDatabase::logDatabaseEvent(const std::string& event,
                                 const std::string& details)
{
    /* Log database events using the error callback if available */
    if (errorCallback_)
    {
        errorCallback_(event, details);
    }

    /* Store event information for debugging */
    lastError_ = "Event: " + event + " - Details: " + details;
    lastErrorTime_ = std::chrono::steady_clock::now();
}

CDatabaseQueryResult CDatabase::processQuery(const std::string& query)
{
    CDatabaseQueryResult result;

    if (!validateQuery(query))
    {
        result.success = false;
        result.message = "Invalid query: " + getLastError();
        return result;
    }

    /* Base implementation - should be overridden by derived classes */
    result.success = false;
    result.message =
        "Base processQuery not implemented - override in derived class";
    return result;
}

CDatabaseQueryResult
CDatabase::processParameterizedQuery(const std::string& query,
                                     const std::vector<std::string>& parameters)
{
    CDatabaseQueryResult result;

    if (!validateQuery(query))
    {
        result.success = false;
        result.message = "Invalid query: " + getLastError();
        return result;
    }

    if (parameters.empty())
    {
        result.success = false;
        result.message = "No parameters provided for parameterized query";
        return result;
    }

    /* Base implementation - should be overridden by derived classes */
    result.success = false;
    result.message = "Base processParameterizedQuery not implemented - "
                     "override in derived class";
    return result;
}

bool CDatabase::validateQuery(const std::string& query)
{
    if (query.empty())
    {
        lastError_ = "Query cannot be empty";
        lastErrorTime_ = std::chrono::steady_clock::now();
        return false;
    }

    /* Basic validation - check for dangerous patterns */
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                   ::tolower);

    /* Check for potentially dangerous patterns */
    std::vector<std::string> dangerousPatterns = {
        "drop database", "drop table",  "truncate",    "delete from",
        "update set",    "insert into", "create user", "drop user",
        "grant",         "revoke"};

    for (const auto& pattern : dangerousPatterns)
    {
        if (lowerQuery.find(pattern) != std::string::npos)
        {
            lastError_ =
                "Query contains potentially dangerous pattern: " + pattern;
            lastErrorTime_ = std::chrono::steady_clock::now();
            return false;
        }
    }

    return true;
}

std::string CDatabase::sanitizeQuery(const std::string& query)
{
    if (query.empty())
    {
        return query;
    }

    std::string sanitized = query;

    /* Escape single quotes to prevent SQL injection */
    size_t pos = 0;
    while ((pos = sanitized.find("'", pos)) != std::string::npos)
    {
        sanitized.replace(pos, 1, "''");
        pos += 2;
    }

    /* Escape backslashes */
    pos = 0;
    while ((pos = sanitized.find("\\", pos)) != std::string::npos)
    {
        sanitized.replace(pos, 1, "\\\\");
        pos += 2;
    }

    return sanitized;
}

bool CDatabase::validateTransactionState()
{
    /* Check if transaction operations are allowed in current state */
    if (status_ != CDatabaseStatus::CONNECTED)
    {
        lastError_ = "Database not connected";
        lastErrorTime_ = std::chrono::steady_clock::now();
        return false;
    }

    return true;
}

void CDatabase::logTransactionEvent(const std::string& event)
{
    /* Log transaction events using the error callback if available */
    if (errorCallback_)
    {
        errorCallback_("TRANSACTION_EVENT", event);
    }

    /* Store event information for debugging */
    lastError_ = "Transaction Event: " + event;
    lastErrorTime_ = std::chrono::steady_clock::now();
}

void CDatabase::initializeDefaults()
{
    /* Initialize with sensible defaults */
    lastError_.clear();
    lastErrorTime_ = std::chrono::steady_clock::now();
}

void CDatabase::cleanupState()
{
    /* Clean up any resources */
    lastError_.clear();
}

std::string CDatabase::buildErrorMessage(const std::string& operation,
                                         const std::string& details)
{
    return "Error in " + operation + ": " + details;
}

bool CDatabase::isTransactionAllowed() const
{
    return status_ == CDatabaseStatus::CONNECTED;
}

} /* namespace FauxDB */
