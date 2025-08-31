/*-------------------------------------------------------------------------
 *
 * IInterfaces.hpp
 *      Interface definitions for FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#pragma once
#include <string>
#include <system_error>

namespace FauxDB
{

/**
 * Log levels for the logging system
 */
enum class CLogLevel
{
	TRACE = 0,
	DEBUG = 1,
	INFO = 2,
	WARN = 3,
	ERROR = 4,
	FATAL = 5
};

/**
 * Interface for logging functionality
 */
class ILogger
{
  public:
	virtual ~ILogger() = default;
// ...existing code...

	/* Core logging interface */
	virtual void log(CLogLevel level, const std::string& message) = 0;
	virtual void setLogLevel(CLogLevel level) = 0;
	virtual CLogLevel getLogLevel() const noexcept = 0;
	virtual std::error_code initialize() = 0;
	virtual void shutdown() noexcept = 0;
};

} /* namespace FauxDB */
