
#include "protocol/CResponseBuilder.hpp"

using namespace FauxDB;
#include "CBsonType.hpp"

#include <algorithm>
#include <expected>
#include <format>
#include <iomanip>
#include <ranges>
#include <span>
#include <sstream>

CResponseBuilder::CResponseBuilder()
    : responseFormat_(CResponseFormat::BSON), protocol_("Document"),
      version_("1.0"), compressionEnabled_(false), responseCount_(0),
      errorCount_(0)
{
}

CResponseBuilder::~CResponseBuilder() = default;

void CResponseBuilder::setResponseFormat(CResponseFormat format)
{
    responseFormat_ = format;
}

void CResponseBuilder::setProtocol(const std::string& protocol)
{
    protocol_ = protocol;
}

void CResponseBuilder::setVersion(const std::string& version)
{
    version_ = version;
}

void CResponseBuilder::setCompression(bool enabled)
{
    compressionEnabled_ = enabled;
}

void CResponseBuilder::setRequestId(uint32_t requestId)
{
    metadata_.requestId = requestId;
}

void CResponseBuilder::setResponseTo(uint32_t responseTo)
{
    metadata_.responseTo = responseTo;
}

void CResponseBuilder::setTimestamp(
    const std::chrono::system_clock::time_point& timestamp)
{
    metadata_.timestamp = timestamp;
}

bool CResponseBuilder::validateResponse(
    const std::vector<uint8_t>& response) const
{
    if (response.empty())
    {
        return false;
    }

    if (!validateMetadata())
    {
        return false;
    }

    return true;
}

std::string CResponseBuilder::getValidationErrors() const
{
    std::stringstream errors;
    if (!validateMetadata())
    {
        errors << "Invalid metadata: protocol or version missing\n";
    }
    return errors.str();
}

size_t CResponseBuilder::getResponseCount() const
{
    return responseCount_;
}

size_t CResponseBuilder::getErrorCount() const
{
    return errorCount_;
}

void CResponseBuilder::resetStatistics()
{
    responseCount_ = 0;
    errorCount_ = 0;
}

std::vector<uint8_t> CResponseBuilder::serializeMetadata() const
{
    std::vector<uint8_t> metadata;

    metadata.push_back(static_cast<uint8_t>(metadata_.status));

    uint16_t messageLength = static_cast<uint16_t>(metadata_.message.length());
    metadata.push_back(static_cast<uint8_t>(messageLength & 0xFF));
    metadata.push_back(static_cast<uint8_t>((messageLength >> 8) & 0xFF));

    metadata.insert(metadata.end(), metadata_.message.begin(),
                    metadata_.message.end());

    metadata.push_back(static_cast<uint8_t>(metadata_.requestId & 0xFF));
    metadata.push_back(static_cast<uint8_t>((metadata_.requestId >> 8) & 0xFF));
    metadata.push_back(
        static_cast<uint8_t>((metadata_.requestId >> 16) & 0xFF));
    metadata.push_back(
        static_cast<uint8_t>((metadata_.requestId >> 24) & 0xFF));

    metadata.push_back(static_cast<uint8_t>(metadata_.responseTo & 0xFF));
    metadata.push_back(
        static_cast<uint8_t>((metadata_.responseTo >> 8) & 0xFF));
    metadata.push_back(
        static_cast<uint8_t>((metadata_.responseTo >> 16) & 0xFF));
    metadata.push_back(
        static_cast<uint8_t>((metadata_.responseTo >> 24) & 0xFF));

    return metadata;
}

std::vector<uint8_t>
CResponseBuilder::compressResponse(const std::vector<uint8_t>& response) const
{
    if (!compressionEnabled_)
    {
        return response;
    }

    std::vector<uint8_t> compressed;
    compressed.reserve(response.size());

    for (const auto& byte : response)
    {
        compressed.push_back(byte ^ 0xFF);
    }

    return compressed;
}

std::vector<uint8_t>
CResponseBuilder::decompressResponse(const std::vector<uint8_t>& response) const
{
    if (!compressionEnabled_)
    {
        return response;
    }

    std::vector<uint8_t> decompressed;
    decompressed.reserve(response.size());

    for (const auto& byte : response)
    {
        decompressed.push_back(byte ^ 0xFF);
    }

    return decompressed;
}

bool CResponseBuilder::validateMetadata() const
{
    if (metadata_.protocol.empty())
    {
        return false;
    }

    if (metadata_.version.empty())
    {
        return false;
    }

    return true;
}

std::string
CResponseBuilder::buildErrorMessage(const std::string& operation,
                                    const std::string& details) const
{
    std::stringstream ss;
    ss << operation << " failed: " << details;
    return ss.str();
}

std::vector<uint8_t>
CResponseBuilder::buildResponse(const FauxDB::CQueryResult& result)
{
    try
    {
        switch (responseFormat_)
        {
        case CResponseFormat::BSON:
        {
            CBSONResponseBuilder bsonBuilder;
            return bsonBuilder.buildBSONResponse(result);
        }
        case CResponseFormat::JSON:
        {
            CBSONResponseBuilder bsonBuilder;
            return bsonBuilder.buildBSONResponse(result);
        }
        default:
        {
            CBSONResponseBuilder bsonBuilder;
            return bsonBuilder.buildBSONResponse(result);
        }
        }
    }
    catch (const std::exception& e)
    {
        return buildErrorResponse(e.what(), -1);
    }
}

std::vector<uint8_t>
CResponseBuilder::buildErrorResponse(const std::string& errorMessage,
                                     int errorCode)
{
    try
    {
        metadata_.status = CResponseStatus::Error;
        metadata_.message = errorMessage;

        std::vector<uint8_t> response = serializeMetadata();

        response.push_back(static_cast<uint8_t>(errorCode & 0xFF));
        response.push_back(static_cast<uint8_t>((errorCode >> 8) & 0xFF));
        response.push_back(static_cast<uint8_t>((errorCode >> 16) & 0xFF));
        response.push_back(static_cast<uint8_t>((errorCode >> 24) & 0xFF));

        errorCount_++;
        return response;
    }
    catch (const std::exception& e)
    {
        std::vector<uint8_t> fallback;
        fallback.push_back(static_cast<uint8_t>(CResponseStatus::Error));
        fallback.push_back(0x00);
        fallback.push_back(0x00);
        return fallback;
    }
}

std::vector<uint8_t>
CResponseBuilder::buildSuccessResponse(const std::string& message)
{
    try
    {
        metadata_.status = CResponseStatus::Success;
        metadata_.message = message;

        std::vector<uint8_t> response = serializeMetadata();
        responseCount_++;
        return response;
    }
    catch (const std::exception& e)
    {
        return buildErrorResponse(e.what(), -1);
    }
}

std::vector<uint8_t> CResponseBuilder::buildEmptyResponse()
{
    try
    {
        metadata_.status = CResponseStatus::Success;
        metadata_.message = "";

        std::vector<uint8_t> response = serializeMetadata();
        responseCount_++;
        return response;
    }
    catch (const std::exception& e)
    {
        return buildErrorResponse(e.what(), -1);
    }
}

std::vector<uint8_t> CBSONResponseBuilder::serializeBSONDocument(
    const std::unordered_map<std::string, std::string>& data) const
{
    std::vector<uint8_t> bson;

    bson.resize(4);

    for (const auto& [key, value] : data)
    {
        bson.push_back(0x02);

        bson.insert(bson.end(), key.begin(), key.end());
        bson.push_back(0x00);

        uint32_t valueLength = static_cast<uint32_t>(value.length());
        bson.push_back(static_cast<uint8_t>(valueLength & 0xFF));
        bson.push_back(static_cast<uint8_t>((valueLength >> 8) & 0xFF));
        bson.push_back(static_cast<uint8_t>((valueLength >> 16) & 0xFF));
        bson.push_back(static_cast<uint8_t>((valueLength >> 24) & 0xFF));

        bson.insert(bson.end(), value.begin(), value.end());
    }

    bson.push_back(0x00);

    uint32_t documentSize = static_cast<uint32_t>(bson.size());
    bson[0] = static_cast<uint8_t>(documentSize & 0xFF);
    bson[1] = static_cast<uint8_t>((documentSize >> 8) & 0xFF);
    bson[2] = static_cast<uint8_t>((documentSize >> 16) & 0xFF);
    bson[3] = static_cast<uint8_t>((documentSize >> 24) & 0xFF);

    return bson;
}

std::vector<uint8_t> CBSONResponseBuilder::serializeBSONArray(
    const std::vector<std::string>& items) const
{
    std::vector<uint8_t> bson;

    bson.resize(4);

    for (size_t i = 0; i < items.size(); ++i)
    {
        bson.push_back(0x02);

        std::string indexStr = std::to_string(i);
        bson.insert(bson.end(), indexStr.begin(), indexStr.end());
        bson.push_back(0x00);

        uint32_t valueLength = static_cast<uint32_t>(items[i].length());
        bson.push_back(static_cast<uint8_t>(valueLength & 0xFF));
        bson.push_back(static_cast<uint8_t>((valueLength >> 8) & 0xFF));
        bson.push_back(static_cast<uint8_t>((valueLength >> 16) & 0xFF));
        bson.push_back(static_cast<uint8_t>((valueLength >> 24) & 0xFF));

        bson.insert(bson.end(), items[i].begin(), items[i].end());
    }

    bson.push_back(0x00);

    uint32_t arraySize = static_cast<uint32_t>(bson.size());
    bson[0] = static_cast<uint8_t>(arraySize & 0xFF);
    bson[1] = static_cast<uint8_t>((arraySize >> 8) & 0xFF);
    bson[2] = static_cast<uint8_t>((arraySize >> 16) & 0xFF);
    bson[3] = static_cast<uint8_t>((arraySize >> 24) & 0xFF);

    return bson;
}

uint32_t
CBSONResponseBuilder::calculateBSONSize(const std::vector<uint8_t>& data) const
{
    return static_cast<uint32_t>(data.size());
}

CJSONResponseBuilder::CJSONResponseBuilder() : CResponseBuilder()
{
    setResponseFormat(CResponseFormat::JSON);
}

CJSONResponseBuilder::~CJSONResponseBuilder() = default;

std::vector<uint8_t>
CJSONResponseBuilder::buildJSONResponse(const FauxDB::CQueryResult& result)
{
    try
    {
        std::string jsonStr;

        if (result.success)
        {
            jsonStr = buildJSONDocument(
                {{"status", "success"},
                 {"rowsAffected", std::to_string(result.rowsAffected)},
                 {"columns", std::to_string(result.columns.size())}});
        }
        else
        {
            jsonStr = buildJSONDocument(
                {{"status", "error"},
                 {"errorMessage", result.errorMessage},
                 {"rowsAffected", std::to_string(result.rowsAffected)}});
        }

        responseCount_++;
        return std::vector<uint8_t>(jsonStr.begin(), jsonStr.end());
    }
    catch (const std::exception& e)
    {
        errorCount_++;
        return buildErrorResponse(e.what(), -1);
    }
}

std::string CJSONResponseBuilder::buildJSONDocument(
    const std::unordered_map<std::string, std::string>& data)
{
    return serializeJSONDocument(data);
}

std::string
CJSONResponseBuilder::buildJSONArray(const std::vector<std::string>& items)
{
    return serializeJSONArray(items);
}

std::string CJSONResponseBuilder::serializeJSONDocument(
    const std::unordered_map<std::string, std::string>& data) const
{
    std::string json = "{";
    bool first = true;

    for (const auto& [key, value] : data)
    {
        if (!first)
        {
            json += ",";
        }
        std::stringstream ss;
        ss << "\"" << escapeJSONString(key) << "\":\""
           << escapeJSONString(value) << "\"";
        json += ss.str();
        first = false;
    }

    json += "}";
    return json;
}

std::string CJSONResponseBuilder::serializeJSONArray(
    const std::vector<std::string>& items) const
{
    std::string json = "[";

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i > 0)
        {
            json += ",";
        }
        std::stringstream ss;
        ss << "\"" << escapeJSONString(items[i]) << "\"";
        json += ss.str();
    }

    json += "]";
    return json;
}

std::string CJSONResponseBuilder::escapeJSONString(const std::string& str) const
{
    std::string escaped;
    escaped.reserve(str.length());

    for (char c : str)
    {
        switch (c)
        {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += c;
            break;
        }
    }

    return escaped;
}

CBSONResponseBuilder::CBSONResponseBuilder() = default;

CBSONResponseBuilder::~CBSONResponseBuilder() = default;

std::vector<uint8_t>
CBSONResponseBuilder::buildBSONResponse(const CQueryResult& result)
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    if (result.success)
    {
        bson.addBool("ok", true);
        bson.addInt32("rowsAffected", result.rowsAffected);
        bson.addInt32("columns", static_cast<int32_t>(result.columns.size()));
    }
    else
    {
        bson.addBool("ok", false);
        bson.addString("error", result.errorMessage);
    }

    bson.endDocument();
    return bson.getDocument();
}

std::vector<uint8_t> CBSONResponseBuilder::buildEmptyResponse()
{
    FauxDB::CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.endDocument();
    return bson.getDocument();
}

std::vector<uint8_t>
CBSONResponseBuilder::buildErrorResponse(const std::string& message, int code)
{
    FauxDB::CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addBool("ok", false);
    bson.addInt32("code", code);
    bson.addString("error", message);
    bson.endDocument();
    return bson.getDocument();
}

std::vector<uint8_t>
CBSONResponseBuilder::buildSuccessResponse(const std::string& message)
{
    FauxDB::CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addBool("ok", true);
    bson.addString("message", message);
    bson.endDocument();
    return bson.getDocument();
}

std::vector<uint8_t>
CBSONResponseBuilder::buildResponse(const CQueryResult& result)
{
    return buildBSONResponse(result);
}

std::vector<uint8_t>
CBSONResponseBuilder::buildBSONArray(const std::vector<std::string>& items)
{
    FauxDB::CBsonType bson;
    bson.initialize();
    bson.beginArray("items");
    for (const auto& item : items)
    {
        bson.addString("", item);
    }
    bson.endArray();
    return bson.getDocument();
}

std::vector<uint8_t> CBSONResponseBuilder::buildBSONDocument(
    const std::unordered_map<std::string, std::string>& data)
{
    FauxDB::CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    for (const auto& [key, value] : data)
    {
        bson.addString(key, value);
    }
    bson.endDocument();
    return bson.getDocument();
}

std::vector<uint8_t>
CJSONResponseBuilder::buildResponse(const CQueryResult& result)
{
    return buildJSONResponse(result);
}

std::vector<uint8_t> CJSONResponseBuilder::buildEmptyResponse()
{
    std::string json = "{}";
    return std::vector<uint8_t>(json.begin(), json.end());
}

std::vector<uint8_t>
CJSONResponseBuilder::buildErrorResponse(const std::string& errorMessage,
                                         int errorCode)
{
    std::string json = "{\"error\":\"" + errorMessage +
                       "\",\"code\":" + std::to_string(errorCode) + "}";
    return std::vector<uint8_t>(json.begin(), json.end());
}

std::vector<uint8_t>
CJSONResponseBuilder::buildSuccessResponse(const std::string& message)
{
    std::string json = "{\"ok\":1,\"message\":\"" + message + "\"}";
    return std::vector<uint8_t>(json.begin(), json.end());
}
