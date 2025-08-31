/*-------------------------------------------------------------------------
 *
 * CParser.hpp
 *      Base parser class for FauxDB.
 *      Abstract base class for all parsing operations.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace FauxDB
{

/**
 * Parser result status
 */
enum class CParserStatus : uint8_t
{
	Success = 0,
	Error = 1,
	Incomplete = 2,
	InvalidFormat = 3,
	UnsupportedType = 4,
	MemoryError = 5,
	Timeout = 6
};

/**
 * Parser result structure
 */
struct CParserResult
{
	CParserStatus status;
	std::string message;
	std::vector<uint8_t> data;
	std::size_t bytesProcessed;
	std::size_t totalBytes;

	CParserResult()
		: status(CParserStatus::Success), bytesProcessed(0), totalBytes(0)
	{
	}
};

/**
 * Base parser class
 * Abstract base class for all parsing operations
 */
class CParser
{
  public:
	CParser();
	virtual ~CParser();

	/* Core parsing interface */
	virtual CParserResult parse(const std::string& input) = 0;
	virtual CParserResult parse(const std::vector<uint8_t>& input) = 0;
	virtual CParserResult parse(const uint8_t* input, std::size_t length) = 0;

	/* Parser state management */
	virtual void reset();
	virtual void clear();
	virtual bool isReady() const;
	virtual std::size_t getBufferSize() const;
	virtual bool initialize();
	virtual void shutdown();
	virtual bool isInitialized() const;
	virtual std::vector<uint8_t> getParseBuffer() const;

	/* Message parsing utilities */
	virtual bool parseMessage(const std::vector<uint8_t>& data);
	virtual void clearParseBuffer();
	virtual bool hasCompleteMessage() const;
	virtual std::vector<uint8_t> extractMessage();

	/* Parser configuration */
	virtual void setMaxBufferSize(std::size_t maxSize);
	virtual void setTimeout(std::chrono::milliseconds timeout);
	virtual void setStrictMode(bool strict);
	virtual void setDebugMode(bool debug);

	/* Parser validation */
	virtual bool validateInput(const std::string& input) = 0;
	virtual bool validateInput(const std::vector<uint8_t>& input) = 0;
	virtual bool validateInput(const uint8_t* input, std::size_t length) = 0;

	/* Parser analysis */
	virtual std::size_t estimateOutputSize(const std::string& input) const = 0;
	virtual std::size_t estimateOutputSize(const std::vector<uint8_t>& input) const = 0;
	virtual std::string getParserInfo() const = 0;

	/* Error handling */
	virtual std::string getLastError() const;
	virtual CParserStatus getLastStatus() const;
	virtual void clearErrors();

  protected:
	/* Parser state */
	std::vector<uint8_t> buffer_;
	std::vector<uint8_t> parseBuffer_;
	std::size_t maxBufferSize_;
	std::chrono::milliseconds timeout_;
	bool strictMode_;
	bool debugMode_;
	bool isInitialized_;

	/* Error state */
	mutable std::string lastError_;
	mutable CParserStatus lastStatus_;

	/* Protected utility methods */
	virtual void setError(const std::string& error, CParserStatus status);
	virtual bool checkBufferSize(std::size_t requiredSize);
	virtual void resizeBuffer(std::size_t newSize);
	virtual bool validateBuffer();

	/* Timing utilities */
	virtual bool checkTimeout() const;
	virtual void startTimer();
	virtual void stopTimer();

  private:
	/* Private state */
	std::chrono::steady_clock::time_point startTime_;
	std::chrono::steady_clock::time_point endTime_;

	/* Private utility methods */
	void initializeDefaults();
	void cleanupBuffer();
};

} /* namespace FauxDB */
