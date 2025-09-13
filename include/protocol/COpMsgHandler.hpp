/*-------------------------------------------------------------------------
 *
 * COpMsgHandler.hpp
 *      OP_MSG message handler for MongoDB wire protocol.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once
#include "CBsonType.hpp"
#include "CDocumentWireProtocol.hpp"
#include "IInterfaces.hpp"
#include "database/CPGConnectionPooler.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
namespace FauxDB
{

using namespace FauxDB;

enum class SectionKind : uint8_t
{
    DOCUMENT = 0x00,         /* Single document */
    DOCUMENT_SEQUENCE = 0x01 /* Document sequence */
};

struct OpMsgSection
{
    SectionKind kind;
    std::string identifier; /* For document sequences */
    std::vector<std::vector<uint8_t>> documents;
};

struct OpMsgCommand
{
    std::string commandName;
    std::string database;
    std::vector<uint8_t> commandBody;
    std::vector<OpMsgSection> sections;
    int32_t flagBits;
    bool checksumPresent;
    uint32_t checksum; /* CRC32C if present */
};

class COpMsgHandler
{
  public:
    COpMsgHandler();

    std::vector<uint8_t> processMessage(const std::vector<uint8_t>& message);
    OpMsgCommand parseMessage(const std::vector<uint8_t>& message);

    std::vector<uint8_t> handleHello(const OpMsgCommand& command);
    std::vector<uint8_t> handlePing(const OpMsgCommand& command);
    std::vector<uint8_t> handleBuildInfo(const OpMsgCommand& command);
    std::vector<uint8_t> handleGetParameter(const OpMsgCommand& command);
    std::vector<uint8_t> handleFind(const OpMsgCommand& command);
    std::vector<uint8_t> handleInsert(const OpMsgCommand& command);
    std::vector<uint8_t> handleUpdate(const OpMsgCommand& command);
    std::vector<uint8_t> handleDelete(const OpMsgCommand& command);
    std::vector<uint8_t> handleGetMore(const OpMsgCommand& command);
    std::vector<uint8_t> handleKillCursors(const OpMsgCommand& command);

    std::vector<uint8_t>
    buildSuccessResponse(const std::vector<uint8_t>& response);
    std::vector<uint8_t> buildErrorResponse(const std::string& error, int code,
                                            const std::string& codeName);
    std::vector<uint8_t>
    buildCursorResponse(const std::string& ns, int64_t cursorId,
                        const std::vector<std::vector<uint8_t>>& batch);

    void setConnectionPooler(std::shared_ptr<CPGConnectionPooler> pooler);

  private:
    bool parseSections(const uint8_t*& data, size_t& remaining,
                       std::vector<OpMsgSection>& sections);
    OpMsgSection parseDocumentSection(const uint8_t*& data, size_t& remaining);
    OpMsgSection parseDocumentSequenceSection(const uint8_t*& data,
                                              size_t& remaining);
    void parseCommandFromSections(OpMsgCommand& command);

    std::vector<uint8_t> routeCommand(const OpMsgCommand& command);

    bool validateCommand(const OpMsgCommand& command);
    bool validateChecksum(const std::vector<uint8_t>& message,
                          const OpMsgCommand& command);

    uint32_t calculateCRC32C(const uint8_t* data, size_t size);
    std::vector<uint8_t> serializeBsonDocument(const std::vector<uint8_t>& doc);

    std::shared_ptr<CPGConnectionPooler> connectionPooler_;
    std::shared_ptr<ILogger> logger_;
};

} /* namespace FauxDB */
