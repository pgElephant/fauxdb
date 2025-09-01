/*-------------------------------------------------------------------------
 *
 * CResponseBuilder.hpp
 *      Response builder interface for FauxDB.
 *      Handles building responses for different protocols and formats.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "../CTypes.hpp"

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace FauxDB
{

/**
 * Response format types
 */
enum class CResponseFormat : uint8_t
{
    BSON = 0,
    JSON = 1,
    XML = 2,
    PlainText = 3
};

/**
 * Response status
 */
enum class CResponseStatus : uint8_t
{
    Success = 0,
    Error = 1,
    Warning = 2,
    Info = 3
};

/**
 * Response metadata
 */
struct CResponseMetadata
{
    CResponseStatus status;
    std::string message;
    uint32_t requestId;
    uint32_t responseTo;
    std::chrono::system_clock::time_point timestamp;
    std::string protocol;
    std::string version;

    CResponseMetadata()
        : status(CResponseStatus::Success), requestId(0), responseTo(0),
          timestamp(std::chrono::system_clock::now())
    {
    }
};

/**
 * Response builder interface
 * Builds responses for different protocols and formats
 */
class CResponseBuilder
{
  public:
    CResponseBuilder();
    virtual ~CResponseBuilder();

    /* Response building */
    virtual std::vector<uint8_t> buildResponse(const CQueryResult& result) = 0;
    virtual std::vector<uint8_t>
    buildErrorResponse(const std::string& errorMessage, int errorCode) = 0;
    virtual std::vector<uint8_t>
    buildSuccessResponse(const std::string& message) = 0;
    virtual std::vector<uint8_t> buildEmptyResponse() = 0;

    /* Response configuration */
    virtual void setResponseFormat(CResponseFormat format);
    virtual void setProtocol(const std::string& protocol);
    virtual void setVersion(const std::string& version);
    virtual void setCompression(bool enabled);

    /* Response metadata */
    virtual void setRequestId(uint32_t requestId);
    virtual void setResponseTo(uint32_t responseTo);
    virtual void
    setTimestamp(const std::chrono::system_clock::time_point& timestamp);

    /* Response validation */
    virtual bool validateResponse(const std::vector<uint8_t>& response) const;
    virtual std::string getValidationErrors() const;

    /* Response statistics */
    virtual size_t getResponseCount() const;
    virtual size_t getErrorCount() const;
    virtual void resetStatistics();

  protected:
    /* Response state */
    CResponseFormat responseFormat_;
    std::string protocol_;
    std::string version_;
    bool compressionEnabled_;

    /* Response metadata */
    CResponseMetadata metadata_;

    /* Response statistics */
    size_t responseCount_;
    size_t errorCount_;

    /* Protected utility methods */
    virtual std::vector<uint8_t> serializeMetadata() const;
    virtual std::vector<uint8_t>
    compressResponse(const std::vector<uint8_t>& response) const;
    virtual std::vector<uint8_t>
    decompressResponse(const std::vector<uint8_t>& response) const;
    virtual bool validateMetadata() const;
    virtual std::string buildErrorMessage(const std::string& operation,
                                          const std::string& details) const;
};

/**
 * BSON response builder
 * Builds BSON format responses
 */
class CBSONResponseBuilder : public CResponseBuilder
{
  public:
    CBSONResponseBuilder();
    virtual ~CBSONResponseBuilder();

    /* BSON-specific response building */
    virtual std::vector<uint8_t> buildBSONResponse(const CQueryResult& result);
    virtual std::vector<uint8_t>
    buildBSONDocument(const std::unordered_map<std::string, std::string>& data);
    virtual std::vector<uint8_t>
    buildBSONArray(const std::vector<std::string>& items);

    /* Pure virtual method implementations */
    std::vector<uint8_t> buildResponse(const CQueryResult& result) override;
    std::vector<uint8_t> buildErrorResponse(const std::string& errorMessage,
                                            int errorCode) override;
    std::vector<uint8_t>
    buildSuccessResponse(const std::string& message) override;
    std::vector<uint8_t> buildEmptyResponse() override;

  protected:
    /* BSON-specific utility methods */
    virtual std::vector<uint8_t> serializeBSONDocument(
        const std::unordered_map<std::string, std::string>& data) const;
    virtual std::vector<uint8_t>
    serializeBSONArray(const std::vector<std::string>& items) const;
    virtual uint32_t calculateBSONSize(const std::vector<uint8_t>& data) const;
};

/**
 * JSON response builder
 * Builds JSON format responses
 */
class CJSONResponseBuilder : public CResponseBuilder
{
  public:
    CJSONResponseBuilder();
    virtual ~CJSONResponseBuilder();

    // Pure virtual overrides from CResponseBuilder
    std::vector<uint8_t> buildResponse(const CQueryResult& result) override;
    std::vector<uint8_t> buildErrorResponse(const std::string& errorMessage,
                                            int errorCode) override;
    std::vector<uint8_t>
    buildSuccessResponse(const std::string& message) override;
    std::vector<uint8_t> buildEmptyResponse() override;

    /* JSON-specific response building */
    virtual std::vector<uint8_t> buildJSONResponse(const CQueryResult& result);
    virtual std::string
    buildJSONDocument(const std::unordered_map<std::string, std::string>& data);
    virtual std::string buildJSONArray(const std::vector<std::string>& items);

  protected:
    /* JSON-specific utility methods */
    virtual std::string serializeJSONDocument(
        const std::unordered_map<std::string, std::string>& data) const;
    virtual std::string
    serializeJSONArray(const std::vector<std::string>& items) const;
    virtual std::string escapeJSONString(const std::string& str) const;
};

/**
 * Response builder factory
 * Creates response builders for different formats
 */
class CResponseBuilderFactory
{
  public:
    CResponseBuilderFactory();
    ~CResponseBuilderFactory() = default;

    /* Factory methods */
    virtual std::unique_ptr<CResponseBuilder>
    createResponseBuilder(CResponseFormat format);
    virtual std::unique_ptr<CResponseBuilder> createBSONResponseBuilder();
    virtual std::unique_ptr<CResponseBuilder> createJSONResponseBuilder();
    virtual std::unique_ptr<CResponseBuilder> createXMLResponseBuilder();
    virtual std::unique_ptr<CResponseBuilder> createPlainTextResponseBuilder();

    /* Factory configuration */
    virtual void setDefaultFormat(CResponseFormat format);
    virtual CResponseFormat getDefaultFormat() const;

  private:
    CResponseFormat defaultFormat_;
};

} /* namespace FauxDB */
