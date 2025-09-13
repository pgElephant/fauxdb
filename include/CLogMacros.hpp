/*-------------------------------------------------------------------------
 *
 * CLogMacros.hpp
 *      Global logging macros for FauxDB
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CLogger.hpp"

/* Global logging macro that handles logger null checks */
#define elog(loglevel, ...)                                                    \
    do                                                                         \
    {                                                                          \
        if (logger_)                                                           \
        {                                                                      \
            logger_->log(loglevel, __VA_ARGS__);                               \
        }                                                                      \
    } while (0)

/* Convenience macros for common log levels */
#define debug_log(...) elog(CLogLevel::DEBUG, __VA_ARGS__)
#define error_log(...) elog(CLogLevel::ERROR, __VA_ARGS__)
#define info_log(...) elog(CLogLevel::INFO, __VA_ARGS__)
#define warn_log(...) elog(CLogLevel::WARN, __VA_ARGS__)
#define fatal_log(...) elog(CLogLevel::FATAL, __VA_ARGS__)
