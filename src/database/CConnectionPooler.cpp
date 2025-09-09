/*-------------------------------------------------------------------------
 *
 * CConnectionPooler.cpp
 *      Implementation of connection pooling for FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

// System headers
#include <algorithm>
#include <chrono>
#include <stdexcept>

// Library headers

// Project headers
#include "CConnectionPooler.hpp"

using namespace std;
namespace FauxDB
{

/*-------------------------------------------------------------------------
 * CConnectionPooler implementation
 *-------------------------------------------------------------------------*/

CConnectionPooler::CConnectionPooler()
    : isRunning_(false), startTime_(std::chrono::steady_clock::now()),
      lastMaintenance_(std::chrono::steady_clock::now()),
      lastValidation_(std::chrono::steady_clock::now())
{
    initializeDefaults();
}

CConnectionPooler::~CConnectionPooler()
{
    /* Note: Don't call pure virtual functions in destructor */
}

void CConnectionPooler::updateStats(bool success,
                                    std::chrono::milliseconds responseTime)
{
    stats_.totalRequests++;
    if (success)
    {
        stats_.successfulRequests++;
    }
    else
    {
        stats_.failedRequests++;
    }

    calculateAverageResponseTime(responseTime);
}

bool CConnectionPooler::shouldCreateConnection() const
{
    return stats_.totalConnections < config_.maxConnections &&
           stats_.availableConnections < config_.minConnections;
}

bool CConnectionPooler::shouldRemoveConnection() const
{
    return stats_.totalConnections > config_.minConnections &&
           stats_.availableConnections > config_.minConnections;
}

void CConnectionPooler::logConnectionEvent(const std::string& event,
                                           const std::string& details)
{
    // Log connection pool events for monitoring and debugging.
    // This base implementation can be overridden by derived classes.

    // Store event information for statistics.
    if (event == "connection_acquired")
    {
        stats_.successfulRequests++;
    }
    else if (event == "connection_failed")
    {
        stats_.failedRequests++;
    }

    // Use logger if available.
    if (logger_)
    {
        // Use INFO for normal pool events, ERROR for failure events.
        if (event == "INITIALIZATION_FAILED" ||
            event == "INITIALIZATION_ERROR" ||
            event == "CONNECTION_CREATE_ERROR" ||
            event == "CONNECTION_TIMEOUT" || event == "CONNECTION_ADD_ERROR" ||
            event == "CONNECTION_REMOVE_ERROR")
        {
            logger_->log(CLogLevel::ERROR, details + ".");
        }
        else
        {
            logger_->log(CLogLevel::INFO, details + ".");
        }
    }
}

void CConnectionPooler::performMaintenance()
{
    if (!isTimeForMaintenance())
    {
        return;
    }

    cleanupBrokenConnections();
    validateConnections();
    adjustPoolSize();

    lastMaintenance_ = std::chrono::steady_clock::now();
}

void CConnectionPooler::cleanupBrokenConnections()
{
    /* Base implementation - should be overridden by derived classes */
}

void CConnectionPooler::validateConnections()
{
    if (!isTimeForValidation())
    {
        return;
    }

    /* Base implementation - should be overridden by derived classes */
    lastValidation_ = std::chrono::steady_clock::now();
}

void CConnectionPooler::adjustPoolSize()
{
    /* Base implementation - should be overridden by derived classes */
}

void CConnectionPooler::initializeDefaults()
{
    /* Set default configuration */
    config_.maxConnections = 10;
    config_.minConnections = 2;
    config_.connectionTimeout = std::chrono::milliseconds(30000);
    config_.idleTimeout = std::chrono::milliseconds(60000);
    config_.validationInterval = 30000;

    /* Initialize stats */
    stats_ = CConnectionPoolStats();
}

void CConnectionPooler::calculateAverageResponseTime(
    std::chrono::milliseconds responseTime)
{
    if (stats_.totalRequests == 0)
    {
        stats_.averageResponseTime = responseTime;
    }
    else
    {
        stats_.averageResponseTime = std::chrono::milliseconds(
            (stats_.averageResponseTime.count() + responseTime.count()) / 2);
    }
}

bool CConnectionPooler::isTimeForMaintenance() const
{
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastMaintenance = now - lastMaintenance_;
    return timeSinceLastMaintenance >=
           std::chrono::milliseconds(config_.validationInterval);
}

bool CConnectionPooler::isTimeForValidation() const
{
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastValidation = now - lastValidation_;
    return timeSinceLastValidation >=
           std::chrono::milliseconds(config_.validationInterval);
}

void CConnectionPooler::setLogger(std::shared_ptr<CLogger> logger)
{
    logger_ = logger;
}

} /* namespace FauxDB */
