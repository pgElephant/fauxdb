/*-------------------------------------------------------------------------
 *
 * CMessageParser.hpp
 *      Protocol message parser implementation.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once
#include "../CTypes.hpp"
#include "../IInterfaces.hpp"
#include "CDocumentHeader.hpp"
#include "IMessageParser.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace FauxDB
{

class CMessageParser : public IMessageParser
{
  public:
    CMessageParser();
    virtual ~CMessageParser();

    std::error_code
    parseMessage(const std::vector<uint8_t>& rawMessage) override;
    bool isValidMessage() const noexcept override;
    std::string getErrorMessage() const override;
    void reset() noexcept override;

    // Additional methods referenced in implementation
    CDocumentHeader getParsedHeader() const;
    std::string getParsedCollection() const;
    std::string getParsedDatabase() const;
    std::string getParsedQuery() const;
    std::string getParsedDocument() const;
    uint32_t getParsedRequestId() const;
    uint32_t getParsedResponseTo() const;

    // Additional parsing methods
    std::error_code parseQueryMessage(const std::vector<uint8_t>& message);
    std::error_code parseCommandMessage(const std::vector<uint8_t>& message);
    std::error_code parseInsertMessage(const std::vector<uint8_t>& message);
    std::error_code parseUpdateMessage(const std::vector<uint8_t>& message);
    std::error_code parseDeleteMessage(const std::vector<uint8_t>& message);
    std::error_code parseMessageHeader(const std::vector<uint8_t>& rawMessage);
    std::error_code parseMessageBody(const std::vector<uint8_t>& rawMessage);

  private:
    bool messageValid_;
    std::string errorMessage_;
    CDocumentHeader parsedHeader_;
    std::string parsedCollection_;
    std::string parsedDatabase_;
    std::string parsedQuery_;
    std::string parsedDocument_;
    uint32_t parsedRequestId_;
    uint32_t parsedResponseTo_;

    // Helper methods
    std::string parseCString(const std::vector<uint8_t>& message,
                             size_t& offset);
    std::vector<uint8_t> parseDocument(const std::vector<uint8_t>& message,
                                       size_t& offset);
    uint32_t readLittleEndian32(const std::vector<uint8_t>& data,
                                size_t offset);
    uint64_t readLittleEndian64(const std::vector<uint8_t>& data,
                                size_t offset);
    bool isValidBSONDocument(const std::vector<uint8_t>& document);
};

class CBSONParser
{
  public:
    CBSONParser();
    ~CBSONParser() = default;
};

class CQueryParser
{
  public:
    CQueryParser();
    ~CQueryParser() = default;
};

class CMessageParserFactory
{
  public:
    enum class ParserType
    {
        Document,
        Redis,
        Memcached,
        Custom
    };

    static std::unique_ptr<IMessageParser> createParser(ParserType type);
    static std::string getParserTypeName(ParserType type);
    static ParserType getParserTypeFromString(const std::string& typeName);
};

} // namespace FauxDB
