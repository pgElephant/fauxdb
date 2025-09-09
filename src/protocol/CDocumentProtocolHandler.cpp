/*-------------------------------------------------------------------------
 *
 * CDocumentProtocolHandler.cpp
 *      MongoDB wire protocol handler for FauxDB.
 *      Implements OP_MSG and OP_QUERY protocol handling with PostgreSQL backend.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "CDocumentProtocolHandler.hpp"

#include "../database/CPGConnectionPooler.hpp"
#include "CBsonType.hpp"
#include "CLogger.hpp"
#include "commands/CAggregateCommand.hpp"
#include "commands/CBuildInfoCommand.hpp"
#include "commands/CCollStatsCommand.hpp"
#include "commands/CCountCommand.hpp"
#include "commands/CCreateCommand.hpp"
#include "commands/CCreateIndexesCommand.hpp"
#include "commands/CDbStatsCommand.hpp"
#include "commands/CDistinctCommand.hpp"
#include "commands/CDropCommand.hpp"
#include "commands/CDropIndexesCommand.hpp"
#include "commands/CExplainCommand.hpp"
#include "commands/CFindAndModifyCommand.hpp"
#include "commands/CHelloCommand.hpp"
#include "commands/CIsMasterCommand.hpp"
#include "commands/CListCollectionsCommand.hpp"
#include "commands/CListDatabasesCommand.hpp"
#include "commands/CListIndexesCommand.hpp"
#include "commands/CPingCommand.hpp"
#include "commands/CServerStatusCommand.hpp"
#include "commands/CWhatsMyUriCommand.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace FauxDB;

namespace FauxDB
{

CDocumentProtocolHandler::CDocumentProtocolHandler()
    : initialized_(false), isRunning_(false), maxBsonSize_(16777216),
      compressionEnabled_(false), checksumEnabled_(false), messageCount_(0),
      errorCount_(0), compressedMessageCount_(0), connectionPooler_(nullptr),
      commandRegistry_(std::make_unique<CCommandRegistry>())
{
    initializeConfiguration();
}

CDocumentProtocolHandler::~CDocumentProtocolHandler()
{
    shutdown();
}

bool CDocumentProtocolHandler::initialize()
{
    if (initialized_)
        return true;
    parser_ = make_unique<CDocumentWireParser>();
    initializeDefaultCommandHandlers();

    if (commandRegistry_)
    {
        commandRegistry_->registerCommand(std::make_unique<CDistinctCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CFindAndModifyCommand>());
        commandRegistry_->registerCommand(std::make_unique<CDropCommand>());
        commandRegistry_->registerCommand(std::make_unique<CCreateCommand>());
        commandRegistry_->registerCommand(std::make_unique<CCountCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CListCollectionsCommand>());
        commandRegistry_->registerCommand(std::make_unique<CExplainCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CAggregateCommand>());
        commandRegistry_->registerCommand(std::make_unique<CDbStatsCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CCollStatsCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CListDatabasesCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CServerStatusCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CCreateIndexesCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CListIndexesCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CDropIndexesCommand>());
        commandRegistry_->registerCommand(std::make_unique<CPingCommand>());
        commandRegistry_->registerCommand(std::make_unique<CHelloCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CBuildInfoCommand>());
        commandRegistry_->registerCommand(std::make_unique<CIsMasterCommand>());
        commandRegistry_->registerCommand(
            std::make_unique<CWhatsMyUriCommand>());
    }

    initialized_ = true;
    return true;
}

void CDocumentProtocolHandler::shutdown()
{
    if (!initialized_)
        return;
    commandHandlers_.clear();
    parser_.reset();
    initialized_ = false;
}

bool CDocumentProtocolHandler::start()
{
    if (!initialized_)
        return false;
    isRunning_ = true;
    return true;
}

void CDocumentProtocolHandler::stop()
{
    if (!isRunning_)
        return;
    isRunning_ = false;
}

bool CDocumentProtocolHandler::isRunning() const
{
    return isRunning_;
}

void CDocumentProtocolHandler::registerCommandHandler(
    const string& command, unique_ptr<IDocumentCommandHandler> handler)
{
    if (handler)
        commandHandlers_[command] = std::move(handler);
}

void CDocumentProtocolHandler::unregisterCommandHandler(const string& command)
{
    commandHandlers_.erase(command);
}

vector<uint8_t> CDocumentProtocolHandler::processDocumentMessage(
    const vector<uint8_t>& buffer, ssize_t bytesRead,
    CResponseBuilder& responseBuilder)
{
    (void)responseBuilder;

    // Sanity check: we must have exactly the full message
    if (buffer.empty() || bytesRead < 21)
    {
        return createErrorWireResponse(0);
    }

    // Extract header (16 bytes)
    int32_t messageLength, requestID, responseTo, opCode;
    std::memcpy(&messageLength, buffer.data(), 4);
    std::memcpy(&requestID, buffer.data() + 4, 4);
    std::memcpy(&responseTo, buffer.data() + 8, 4);
    std::memcpy(&opCode, buffer.data() + 12, 4);

    // Sanity check: assert header.messageLength == bytes_read_total
    if (messageLength != bytesRead)
    {
        return createErrorWireResponse(requestID);
    }

    // Handle both OP_MSG (2013) and OP_QUERY (2004)
    if (opCode == 2013)
    {
        // OP_MSG format - our existing implementation
        // Extract flagBits (4 bytes) and first section kind (1 byte)
        uint32_t flagBits;
        std::memcpy(&flagBits, buffer.data() + 16, 4);
        uint8_t kind = buffer[20];

        // Assert body[0..3] are flagBits and zero for our use
        if (flagBits != 0)
        {
            return createErrorWireResponse(requestID);
        }

        // Assert body[4] is 0x00 (section 0)
        if (kind != 0)
        {
            return createErrorWireResponse(requestID);
        }

        // Parse BSON document starting at offset 21
        if (bytesRead < 25)
            return createErrorWireResponse(requestID);

        int32_t docSize;
        std::memcpy(&docSize, buffer.data() + 21, 4);

        // Assert the BSON document length fits inside the body
        if (21 + docSize > bytesRead)
        {
            return createErrorWireResponse(requestID);
        }

        // Parse command name from first field in BSON
        std::string commandName = parseCommandFromBSON(buffer, 25, docSize - 4);

        // Route to appropriate handler - ALWAYS echo the request's requestID
        if (commandName == "hello" || commandName == "isMaster")
        {
            return createHelloWireResponse(requestID);
        }
        else if (commandName == "ping")
        {
            return createPingWireResponse(requestID);
        }
        else if (commandName == "listDatabases")
        {
            return createListDatabasesWireResponse(requestID);
        }
        else if (commandName == "find")
        {
            // Extract collection name from the find command
            std::string collectionName =
                parseCollectionNameFromBSON(buffer, 25, docSize - 4);
            return createFindResponseFromPostgreSQL(collectionName, requestID);
        }
        else if (commandName == "insert")
        {
            return createInsertWireResponse(requestID);
        }
        else if (commandName == "buildInfo")
        {
            return createBuildInfoWireResponse(requestID);
        }
        else if (commandName == "aggregate")
        {
            return createAggregateWireResponse(requestID);
        }
        else if (commandName == "atlasVersion")
        {
            return createAtlasVersionWireResponse(requestID);
        }
        else if (commandName == "getParameter")
        {
            return createGetParameterWireResponse(requestID);
        }
        else
        {
            // Unknown OP_MSG command, return generic error
            return createErrorWireResponse(requestID);
        }
    }
    else if (opCode == 2004)
    {
        // OP_QUERY format: flags(4) + fullCollectionName + numberToSkip(4) +
        // numberToReturn(4) + query

        if (bytesRead < 20)
            return createErrorWireResponse(requestID);

        // Skip flags (4 bytes)
        size_t offset = 20;

        // Find collection name (null-terminated string)
        size_t collNameStart = offset;
        while (offset < (size_t)bytesRead && buffer[offset] != 0)
            offset++;
        if (offset >= (size_t)bytesRead)
            return createErrorWireResponse(requestID);

        std::string collectionName(
            reinterpret_cast<const char*>(buffer.data() + collNameStart),
            offset - collNameStart);
        offset++; // skip null terminator

        // Skip numberToSkip(4) + numberToReturn(4)
        offset += 8;

        if (offset + 4 >= (size_t)bytesRead)
            return createErrorWireResponse(requestID);

        // Parse BSON query document
        int32_t queryDocSize;
        std::memcpy(&queryDocSize, buffer.data() + offset, 4);

        if (offset + queryDocSize > (size_t)bytesRead)
            return createErrorWireResponse(requestID);

        // Parse command from BSON query
        std::string commandName =
            parseCommandFromBSON(buffer, offset + 4, queryDocSize - 4);

        // For OP_QUERY, we need to respond with OP_REPLY (1) instead of OP_MSG
        if (commandName == "hello" || commandName == "isMaster" ||
            commandName == "ismaster")
        {
            return createHelloOpReplyResponse(requestID);
        }
        else if (commandName == "ping")
        {
            return createPingOpReplyResponse(requestID);
        }
        else if (commandName == "find")
        {
            // Extract collection name from the find command
            std::string collectionName = parseCollectionNameFromBSON(
                buffer, offset + 4, queryDocSize - 4);
            return createFindOpReplyResponseFromPostgreSQL(collectionName,
                                                           requestID);
        }
        else
        {
            return createErrorOpReplyResponse(requestID);
        }
    }
    else
    {
        return createErrorWireResponse(requestID);
    }

    return createErrorWireResponse(requestID);
}

std::string
CDocumentProtocolHandler::parseCommandFromBSON(const vector<uint8_t>& buffer,
                                               size_t offset, size_t remaining)
{
    if (remaining < 2)
        return "";

    uint8_t fieldType = buffer[offset];
    // Accept int32, double, or string as command types
    if (!(fieldType == 0x10 || fieldType == 0x01 || fieldType == 0x02))
        return "";

    // Read field name (command name)
    size_t nameStart = offset + 1;
    size_t nameEnd = nameStart;
    while (nameEnd < offset + remaining && buffer[nameEnd] != 0)
        nameEnd++;

    if (nameEnd >= offset + remaining)
        return "";

    return std::string(reinterpret_cast<const char*>(buffer.data() + nameStart),
                       nameEnd - nameStart);
}

std::string CDocumentProtocolHandler::parseCollectionNameFromBSON(
    const vector<uint8_t>& buffer, size_t offset, size_t remaining)
{
    if (remaining < 2)
        return "";

    uint8_t fieldType = buffer[offset];
    // For find command, we expect the first field to be a string (0x02)
    // containing the collection name
    if (fieldType != 0x02)
        return "";

    // Skip field type
    offset++;

    // Read field name - should be "find"
    size_t nameStart = offset;
    size_t nameEnd = nameStart;
    while (nameEnd < offset + remaining && buffer[nameEnd] != 0)
        nameEnd++;

    if (nameEnd >= offset + remaining)
        return "";

    std::string fieldName(
        reinterpret_cast<const char*>(buffer.data() + nameStart),
        nameEnd - nameStart);
    if (fieldName != "find")
        return "";

    // Skip null terminator
    offset = nameEnd + 1;

    // Read string length (4 bytes)
    if (offset + 4 >= offset + remaining)
        return "";

    int32_t stringLen;
    std::memcpy(&stringLen, buffer.data() + offset, 4);
    offset += 4;

    // Read the collection name string (excluding null terminator)
    if (offset + stringLen - 1 >= offset + remaining)
        return "";

    return std::string(reinterpret_cast<const char*>(buffer.data() + offset),
                       stringLen - 1);
}

vector<uint8_t>
CDocumentProtocolHandler::createHelloWireResponse(int32_t requestID)
{
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addDouble("ok", 1.0);
    b.addBool("helloOk", true);
    b.addBool("isWritablePrimary", true);
    b.addInt32("minWireVersion", 0);
    b.addInt32("maxWireVersion", 17);
    b.addInt32("logicalSessionTimeoutMinutes", 30);
    b.addInt32("maxBsonObjectSize", 16777216);
    b.addInt32("maxMessageSizeBytes", 48000000);
    b.addInt32("maxWriteBatchSize", 100000);
    b.endDocument();

    auto doc = b.getDocument();
    return createWireMessage(1, requestID, doc);
}

vector<uint8_t>
CDocumentProtocolHandler::createPingWireResponse(int32_t requestID)
{
    CBsonType b;
    b.initialize();
    b.beginDocument();

    // Test PostgreSQL connectivity for ping command
    bool pgConnected = false;
    std::string pgStatus = "disconnected";

    if (connectionPooler_)
    {
        try
        {
            // Get a PostgreSQL connection from the pool to test connectivity
            auto connection = connectionPooler_->getPostgresConnection();
            if (connection && connection->database &&
                connection->database->isConnected())
            {
                // Test PostgreSQL readiness with a simple query
                auto result =
                    connection->database->executeQuery("SELECT 1 as is_ready");
                if (result.success && !result.rows.empty())
                {
                    pgConnected = true;
                    pgStatus = "connected and ready";
                }
                else
                {
                    pgStatus = "connected but not ready";
                }
            }
            else
            {
                pgStatus = "connection failed";
            }

            // Return connection to pool
            if (connection)
            {
                connectionPooler_->releasePostgresConnection(connection);
            }
        }
        catch (const std::exception& e)
        {
            pgStatus = "error: " + std::string(e.what());
        }
    }
    else
    {
        pgStatus = "no connection pooler";
    }

    // Build response based on PostgreSQL status
    if (pgConnected)
    {
        b.addDouble("ok", 1.0);
        b.addString("postgresql", pgStatus);
    }
    else
    {
        b.addDouble("ok", 0.0);
        b.addString("postgresql", pgStatus);
        b.addString("errmsg", "PostgreSQL not ready");
    }

    b.endDocument();
    return createWireMessage(1, requestID, b.getDocument());
}

vector<uint8_t>
CDocumentProtocolHandler::createListDatabasesWireResponse(int32_t requestID)
{
    CBsonType b;
    b.initialize();
    b.beginDocument();
    // Create empty databases array
    b.beginArray("databases");
    b.endArray();
    b.addInt32("totalSize", 0);
    b.addDouble("ok", 1.0);
    b.endDocument();
    return createWireMessage(1, requestID, b.getDocument());
}

vector<uint8_t>
CDocumentProtocolHandler::createFindWireResponse(int32_t requestID)
{
    // Create cursor subdocument
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    cursor.addInt64("id", 0);
    cursor.addString("ns", "test.coll");
    cursor.beginArray("firstBatch");
    cursor.endArray();
    cursor.endDocument();

    // Create main response document
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addDouble("ok", 1.0);
    b.addDocument("cursor", cursor);
    b.endDocument();

    return createWireMessage(1, requestID, b.getDocument());
}

vector<uint8_t>
CDocumentProtocolHandler::createInsertWireResponse(int32_t requestID)
{
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addDouble("ok", 1.0);
    b.addInt32("n", 1);
    b.endDocument();
    return createWireMessage(1, requestID, b.getDocument());
}

vector<uint8_t>
CDocumentProtocolHandler::createErrorWireResponse(int32_t requestID)
{
    // Simple error response
    vector<uint8_t> doc;
    uint32_t docLen = 17; // Same as ping response but with ok: 0.0
    doc.resize(docLen);
    size_t pos = 0;

    std::memcpy(doc.data() + pos, &docLen, 4);
    pos += 4;
    doc[pos++] = 0x01;
    std::memcpy(doc.data() + pos, "ok", 3);
    pos += 3;
    uint64_t okVal = 0; // 0.0
    std::memcpy(doc.data() + pos, &okVal, 8);
    pos += 8;
    doc[pos++] = 0x00;

    return createWireMessage(1, requestID, doc);
}

// Helper to create simple { ok: 1.0 } BSON document
std::vector<uint8_t> CDocumentProtocolHandler::createSimpleOkBson()
{
    std::vector<uint8_t> doc(17); // { ok: 1.0 } = 17 bytes total
    size_t pos = 0;

    // Document length
    uint32_t docLen = 17;
    std::memcpy(doc.data() + pos, &docLen, 4);
    pos += 4;

    // "ok": 1.0 (double)
    doc[pos++] = 0x01; // type double
    std::memcpy(doc.data() + pos, "ok", 3);
    pos += 3;                               // name + null
    uint64_t okVal = 0x3FF0000000000000ULL; // 1.0 in IEEE754
    std::memcpy(doc.data() + pos, &okVal, 8);
    pos += 8;

    // Document terminator
    doc[pos++] = 0x00;

    return doc;
}

vector<uint8_t>
CDocumentProtocolHandler::createBuildInfoWireResponse(int32_t requestID)
{
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addString("version", "6.0.0");
    b.addString("gitVersion", "nogit");
    b.addString("allocator", "system");
    b.addString("javascriptEngine", "none");
    b.addString("sysInfo", "fauxdb");
    b.addDouble("ok", 1.0);
    b.endDocument();
    return createWireMessage(1, requestID, b.getDocument());
}

vector<uint8_t>
CDocumentProtocolHandler::createAggregateWireResponse(int32_t requestID)
{
    // Create cursor subdocument
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    cursor.addInt64("id", 0);
    cursor.addString("ns", "admin.$cmd");
    cursor.beginArray("firstBatch");
    cursor.endArray();
    cursor.endDocument();

    // Create main response document
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addDocument("cursor", cursor);
    b.addDouble("ok", 1.0);
    b.endDocument();

    return createWireMessage(1, requestID, b.getDocument());
}

vector<uint8_t>
CDocumentProtocolHandler::createAtlasVersionWireResponse(int32_t requestID)
{
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addString("atlasVersion", "1.0");
    b.addDouble("ok", 1.0);
    b.endDocument();
    return createWireMessage(1, requestID, b.getDocument());
}

// BSON validation helper
static bool validateBson(const std::vector<uint8_t>& doc)
{
    if (doc.size() < 5)
        return false;
    const uint8_t* p = doc.data();
    size_t n = doc.size();
    bson_t* b = bson_new_from_data(p, n);
    if (!b)
        return false;
    bool ok = bson_validate(b, BSON_VALIDATE_NONE, nullptr);
    bson_destroy(b);
    return ok;
}

// Unified OP_MSG wire message creator with validation
std::vector<uint8_t> CDocumentProtocolHandler::createWireMessage(
    int32_t requestID, int32_t responseTo, const std::vector<uint8_t>& bsonDoc)
{
    if (!validateBson(bsonDoc))
    {
        if (logger_)
            logger_->log(
                CLogLevel::ERROR,
                "Invalid BSON document, falling back to error response");
        // Create fallback error response
        CBsonType fallback;
        fallback.initialize();
        fallback.beginDocument();
        fallback.addDouble("ok", 0.0);
        fallback.addString("errmsg", "Internal BSON error");
        fallback.endDocument();
        auto fallbackDoc = fallback.getDocument();
        return createWireMessage(requestID, responseTo, fallbackDoc);
    }

    int32_t messageLength = 16 + 4 + 1 + static_cast<int32_t>(bsonDoc.size());
    std::vector<uint8_t> rsp(messageLength);
    size_t off = 0;

    // Header
    std::memcpy(rsp.data() + off, &messageLength, 4);
    off += 4;
    std::memcpy(rsp.data() + off, &requestID, 4);
    off += 4; // server id
    std::memcpy(rsp.data() + off, &responseTo, 4);
    off += 4;          // echo
    int32_t op = 2013; // OP_MSG
    std::memcpy(rsp.data() + off, &op, 4);
    off += 4;

    // OP_MSG fields
    uint32_t flagBits = 0;
    std::memcpy(rsp.data() + off, &flagBits, 4);
    off += 4;
    rsp[off++] = 0x00; // kind 0
    std::memcpy(rsp.data() + off, bsonDoc.data(), bsonDoc.size());

    return rsp;
}

vector<uint8_t>
CDocumentProtocolHandler::createGetParameterWireResponse(int32_t requestID)
{
    // Build featureCompatibilityVersion subdocument
    CBsonType fcv;
    fcv.initialize();
    fcv.beginDocument();
    fcv.addString("version", "6.0");
    fcv.endDocument();

    // Build main response document
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addDocument("featureCompatibilityVersion", fcv);
    b.addDouble("ok", 1.0);
    b.endDocument();

    return createWireMessage(1, requestID, b.getDocument());
}

// OP_REPLY response creators for legacy wire protocol
vector<uint8_t>
CDocumentProtocolHandler::createHelloOpReplyResponse(int32_t requestID)
{
    // Build BSON document first using CBsonType
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addBool("ismaster", true);
    b.addInt32("minWireVersion", 0);
    b.addInt32("maxWireVersion", 17);
    b.addDouble("ok", 1.0);
    b.endDocument();
    auto bsonDoc = b.getDocument();

    // Create OP_REPLY: 16(header) + 20(OP_REPLY fields) + BSON_length
    uint32_t total = 16 + 20 + static_cast<uint32_t>(bsonDoc.size());
    std::vector<uint8_t> response(total);
    size_t off = 0;

    // Header
    std::memcpy(&response[off], &total, 4);
    off += 4;
    int32_t srvId = 1;
    std::memcpy(&response[off], &srvId, 4);
    off += 4;
    std::memcpy(&response[off], &requestID, 4);
    off += 4;
    int32_t op = 1; // OP_REPLY
    std::memcpy(&response[off], &op, 4);
    off += 4;

    // OP_REPLY fields
    uint32_t flags = 0;
    std::memcpy(&response[off], &flags, 4);
    off += 4;
    uint64_t cursorId = 0;
    std::memcpy(&response[off], &cursorId, 8);
    off += 8;
    uint32_t startingFrom = 0;
    std::memcpy(&response[off], &startingFrom, 4);
    off += 4;
    uint32_t numberReturned = 1;
    std::memcpy(&response[off], &numberReturned, 4);
    off += 4;

    // BSON document
    std::memcpy(&response[off], bsonDoc.data(), bsonDoc.size());

    return response;
}

vector<uint8_t>
CDocumentProtocolHandler::createPingOpReplyResponse(int32_t requestID)
{
    // Simple ping response: { "ok": 1.0 } - 17 bytes BSON
    // Total: 16 + 20 + 17 = 53 bytes
    vector<uint8_t> response = {
        // Header (16 bytes)
        0x35, 0x00, 0x00, 0x00, // messageLength = 53
        0x01, 0x00, 0x00, 0x00, // requestID = 1 (server assigned)
        0x00, 0x00, 0x00,
        0x00, // responseTo = requestID (will be updated below)
        0x01, 0x00, 0x00, 0x00, // opCode = 1 (OP_REPLY)

        // OP_REPLY fields (20 bytes)
        0x00, 0x00, 0x00, 0x00,                         // responseFlags = 0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // cursorID = 0
        0x00, 0x00, 0x00, 0x00,                         // startingFrom = 0
        0x01, 0x00, 0x00, 0x00,                         // numberReturned = 1

        // BSON document: { "ok": 1.0 } (17 bytes)
        0x11, 0x00, 0x00, 0x00,                         // document length = 17
        0x01, 0x6f, 0x6b, 0x00,                         // "ok" field name
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, // 1.0 in IEEE754
        0x00                                            // document terminator
    };

    // Update responseTo field to echo client requestID
    std::memcpy(response.data() + 8, &requestID, 4);

    return response;
}

vector<uint8_t>
CDocumentProtocolHandler::createErrorOpReplyResponse(int32_t requestID)
{
    // Error response: { "ok": 0.0 } - 17 bytes BSON
    // Total: 16 + 20 + 17 = 53 bytes
    vector<uint8_t> response = {
        // Header (16 bytes)
        0x35, 0x00, 0x00, 0x00, // messageLength = 53
        0x01, 0x00, 0x00, 0x00, // requestID = 1 (server assigned)
        0x00, 0x00, 0x00,
        0x00, // responseTo = requestID (will be updated below)
        0x01, 0x00, 0x00, 0x00, // opCode = 1 (OP_REPLY)

        // OP_REPLY fields (20 bytes)
        0x00, 0x00, 0x00, 0x00,                         // responseFlags = 0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // cursorID = 0
        0x00, 0x00, 0x00, 0x00,                         // startingFrom = 0
        0x01, 0x00, 0x00, 0x00,                         // numberReturned = 1

        // BSON document: { "ok": 0.0 } (17 bytes)
        0x11, 0x00, 0x00, 0x00,                         // document length = 17
        0x01, 0x6f, 0x6b, 0x00,                         // "ok" field name
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0.0 in IEEE754
        0x00                                            // document terminator
    };

    // Update responseTo field to echo client requestID
    std::memcpy(response.data() + 8, &requestID, 4);

    return response;
}

vector<uint8_t> CDocumentProtocolHandler::createOpReplyResponse(
    int32_t requestID, const vector<uint8_t>& bsonDocument)
{
    // Calculate total size: 16 (header) + 20 (OP_REPLY fields) + document size
    uint32_t totalSize = 16 + 20 + bsonDocument.size();

    vector<uint8_t> response;
    response.reserve(totalSize);

    // Header (16 bytes)
    response.insert(
        response.end(),
        {
            static_cast<uint8_t>(totalSize & 0xFF),
            static_cast<uint8_t>((totalSize >> 8) & 0xFF),
            static_cast<uint8_t>((totalSize >> 16) & 0xFF),
            static_cast<uint8_t>((totalSize >> 24) & 0xFF), // messageLength
            0x01, 0x00, 0x00, 0x00, // requestID = 1 (server assigned)
            0x00, 0x00, 0x00, 0x00, // responseTo (will be updated below)
            0x01, 0x00, 0x00, 0x00  // opCode = 1 (OP_REPLY)
        });

    // OP_REPLY fields (20 bytes)
    response.insert(response.end(),
                    {
                        0x00, 0x00, 0x00, 0x00, // responseFlags = 0
                        0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, // cursorID = 0
                        0x00, 0x00, 0x00, 0x00, // startingFrom = 0
                        0x01, 0x00, 0x00, 0x00  // numberReturned = 1
                    });

    // Append the BSON document
    response.insert(response.end(), bsonDocument.begin(), bsonDocument.end());

    // Update responseTo field to echo client requestID
    std::memcpy(response.data() + 8, &requestID, 4);

    return response;
}

vector<uint8_t>
CDocumentProtocolHandler::createFindOpReplyResponseFromPostgreSQL(
    const string& collectionName, int32_t requestID)
{
    // Check if connection pooler is available
    if (!connectionPooler_)
    {
        return createErrorOpReplyResponse(requestID);
    }

    // Query PostgreSQL for the collection data
    vector<CBsonType> documents = queryPostgreSQLCollection(collectionName);

    // Create cursor subdocument
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    cursor.addInt64("id", 0);
    cursor.addString("ns", collectionName + ".collection");
    cursor.beginArray("firstBatch");

    // Add documents from PostgreSQL
    for (const auto& doc : documents)
    {
        cursor.addArrayDocument(doc);
    }

    cursor.endArray();
    cursor.endDocument();

    // Create main response document
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addDouble("ok", 1.0);
    b.addDocument("cursor", cursor);
    b.endDocument();

    // Create OP_REPLY response
    return createOpReplyResponse(requestID, b.getDocument());
}

vector<uint8_t> CDocumentProtocolHandler::createFindResponseFromPostgreSQL(
    const string& collectionName, int32_t requestID)
{
    // Check if connection pooler is available
    if (!connectionPooler_)
    {
        return createErrorBsonDocument(
            -10, "PostgreSQL connection pooler not available");
    }

    // Query PostgreSQL for the collection data
    vector<CBsonType> documents = queryPostgreSQLCollection(collectionName);

    // Build response document
    CBsonType bson;
    if (!bson.initialize() || !bson.beginDocument())
    {
        return createErrorBsonDocument(-6, "BSON init failed");
    }

    bson.addDouble("ok", 1.0);

    // Create cursor subdocument
    CBsonType cursorDoc;
    cursorDoc.initialize();
    cursorDoc.beginDocument();
    cursorDoc.addInt64("id", 0); // No cursor needed for simple queries
    cursorDoc.addString("ns", collectionName + ".collection"); // namespace

    // Create firstBatch array with PostgreSQL data
    cursorDoc.beginArray("firstBatch");

    // Add documents from PostgreSQL
    for (const auto& doc : documents)
    {
        cursorDoc.addArrayDocument(doc);
    }

    cursorDoc.endArray();    // end firstBatch
    cursorDoc.endDocument(); // end cursor

    // Add the cursor subdocument to main response
    bson.addDocument("cursor", cursorDoc);

    if (!bson.endDocument())
    {
        return createErrorBsonDocument(-7, "BSON finalize failed");
    }

    // Create a proper MongoDB OP_MSG response
    vector<uint8_t> response;

    // Calculate sizes
    vector<uint8_t> bsonDoc = bson.getDocument();
    int32_t bodySize =
        4 + 1 +
        static_cast<int32_t>(bsonDoc.size()); // flagBits + kind + document
    int32_t messageSize = 16 + bodySize;      // header + body

    // Reserve space
    response.resize(messageSize);
    size_t offset = 0;

    // Write header
    std::memcpy(response.data() + offset, &messageSize, 4);
    offset += 4;
    std::memcpy(response.data() + offset, &requestID, 4);
    offset += 4;
    int32_t responseTo = requestID;
    std::memcpy(response.data() + offset, &responseTo, 4);
    offset += 4;
    int32_t opCode = 2013; // OP_MSG
    std::memcpy(response.data() + offset, &opCode, 4);
    offset += 4;

    // Write body
    uint32_t flagBits = 0;
    std::memcpy(response.data() + offset, &flagBits, 4);
    offset += 4;
    uint8_t kind = 0; // Kind 0 (Body)
    response[offset++] = kind;
    std::memcpy(response.data() + offset, bsonDoc.data(), bsonDoc.size());

    return response;
}

vector<CBsonType> CDocumentProtocolHandler::queryPostgreSQLCollection(
    const string& collectionName)
{
    vector<CBsonType> documents;

    if (logger_)
    {
        logger_->log(CLogLevel::INFO,
                     "Querying PostgreSQL collection: " + collectionName);
    }

    // For now, return a simple test document to verify the flow works
    CBsonType testDoc;
    testDoc.initialize();
    testDoc.beginDocument();
    testDoc.addString("_id", "pg_test_123");
    testDoc.addString("name", "PostgreSQL Test User");
    testDoc.addString("email", "test@postgresql.example");
    testDoc.addString("source", "PostgreSQL");
    testDoc.addString("collection", collectionName);
    testDoc.endDocument();
    documents.push_back(testDoc);

    if (logger_)
    {
        logger_->log(CLogLevel::INFO,
                     "Returning test document for collection: " +
                         collectionName);
    }

    return documents;
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::routeCommand(const CDocumentWireMessage& request)
{
    string cmd = extractCommandName(request);
    auto it = commandHandlers_.find(cmd);
    if (it != commandHandlers_.end())
    {
        return it->second->handleCommand(request);
    }
    return nullptr;
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::createHelloResponse(int32_t requestID)
{
    return CDocumentWireMessage::createHelloResponse(requestID);
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::createBuildInfoResponse(int32_t requestID)
{
    return CDocumentWireMessage::createBuildInfoResponse(requestID);
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::createIsMasterResponse(int32_t requestID)
{
    return CDocumentWireMessage::createIsMasterResponse(requestID);
}

unique_ptr<CDocumentWireMessage> CDocumentProtocolHandler::createErrorResponse(
    int32_t requestID, int32_t errorCode, const string& errorMessage)
{
    (void)requestID;

    auto errorDoc = createErrorBsonDocument(errorCode, errorMessage);

    CDocumentReplyBody replyBody;
    replyBody.document = errorDoc;
    auto msg = make_unique<CDocumentWireMessage>();
    msg->setReplyBody(replyBody);
    return msg;
}

vector<uint8_t> CDocumentProtocolHandler::createBsonDocument(
    const unordered_map<string, string>& fields)
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    for (const auto& [key, value] : fields)
    {
        bson.addString(key, value);
    }
    bson.endDocument();

    // Create proper OP_MSG wire protocol response
    vector<uint8_t> response;
    vector<uint8_t> bsonDoc = bson.getDocument();
    int32_t bodySize = 4 + 1 + static_cast<int32_t>(bsonDoc.size());
    int32_t messageSize = 16 + bodySize;

    response.resize(messageSize);
    size_t offset = 0;

    // Header
    std::memcpy(response.data() + offset, &messageSize, 4);
    offset += 4;
    int32_t requestID = 1;
    std::memcpy(response.data() + offset, &requestID, 4);
    offset += 4;
    std::memcpy(response.data() + offset, &requestID, 4);
    offset += 4;
    int32_t opCode = 2013;
    std::memcpy(response.data() + offset, &opCode, 4);
    offset += 4;

    // Body
    uint32_t flagBits = 0;
    std::memcpy(response.data() + offset, &flagBits, 4);
    offset += 4;
    uint8_t kind = 0;
    response[offset++] = kind;
    std::memcpy(response.data() + offset, bsonDoc.data(), bsonDoc.size());

    return response;
}

vector<uint8_t> CDocumentProtocolHandler::createBsonDocument(
    const unordered_map<string, int32_t>& fields)
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    for (const auto& [key, value] : fields)
    {
        bson.addInt32(key, value);
    }
    bson.endDocument();

    // Create proper OP_MSG wire protocol response
    vector<uint8_t> response;
    vector<uint8_t> bsonDoc = bson.getDocument();
    int32_t bodySize = 4 + 1 + static_cast<int32_t>(bsonDoc.size());
    int32_t messageSize = 16 + bodySize;

    response.resize(messageSize);
    size_t offset = 0;

    // Header
    std::memcpy(response.data() + offset, &messageSize, 4);
    offset += 4;
    int32_t requestID = 1;
    std::memcpy(response.data() + offset, &requestID, 4);
    offset += 4;
    std::memcpy(response.data() + offset, &requestID, 4);
    offset += 4;
    int32_t opCode = 2013;
    std::memcpy(response.data() + offset, &opCode, 4);
    offset += 4;

    // Body
    uint32_t flagBits = 0;
    std::memcpy(response.data() + offset, &flagBits, 4);
    offset += 4;
    uint8_t kind = 0;
    response[offset++] = kind;
    std::memcpy(response.data() + offset, bsonDoc.data(), bsonDoc.size());

    return response;
}

vector<uint8_t> CDocumentProtocolHandler::createBsonDocument(
    const unordered_map<string, double>& fields)
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    for (const auto& [key, value] : fields)
    {
        bson.addDouble(key, value);
    }
    bson.endDocument();

    // Create proper OP_MSG wire protocol response
    vector<uint8_t> response;
    vector<uint8_t> bsonDoc = bson.getDocument();
    int32_t bodySize = 4 + 1 + static_cast<int32_t>(bsonDoc.size());
    int32_t messageSize = 16 + bodySize;

    response.resize(messageSize);
    size_t offset = 0;

    // Header
    std::memcpy(response.data() + offset, &messageSize, 4);
    offset += 4;
    int32_t requestID = 1;
    std::memcpy(response.data() + offset, &requestID, 4);
    offset += 4;
    std::memcpy(response.data() + offset, &requestID, 4);
    offset += 4;
    int32_t opCode = 2013;
    std::memcpy(response.data() + offset, &opCode, 4);
    offset += 4;

    // Body
    uint32_t flagBits = 0;
    std::memcpy(response.data() + offset, &flagBits, 4);
    offset += 4;
    uint8_t kind = 0;
    response[offset++] = kind;
    std::memcpy(response.data() + offset, bsonDoc.data(), bsonDoc.size());

    return response;
}

vector<uint8_t> CDocumentProtocolHandler::createBsonDocument(
    const unordered_map<string, bool>& fields)
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    for (const auto& [key, value] : fields)
    {
        bson.addBool(key, value);
    }
    bson.endDocument();
    return bson.getDocument();
}

void CDocumentProtocolHandler::setConnectionPooler(
    shared_ptr<CPGConnectionPooler> pooler)
{
    connectionPooler_ = pooler;
}

void CDocumentProtocolHandler::setLogger(shared_ptr<ILogger> logger)
{
    logger_ = logger;
}

vector<string> CDocumentProtocolHandler::getSupportedCommands() const
{
    vector<string> commands;
    for (const auto& [cmd, _] : commandHandlers_)
    {
        commands.push_back(cmd);
    }
    return commands;
}

void CDocumentProtocolHandler::initializeDefaultCommandHandlers()
{
    commandHandlers_["hello"] = make_unique<CHelloCommandHandler>();
    commandHandlers_["buildInfo"] = make_unique<CBuildInfoCommandHandler>();
    commandHandlers_["isMaster"] = make_unique<CIsMasterCommandHandler>();
}

void CDocumentProtocolHandler::initializeConfiguration()
{
    maxBsonSize_ = 16777216;
    compressionEnabled_ = false;
    checksumEnabled_ = false;
}

string CDocumentProtocolHandler::extractCommandName(
    const CDocumentWireMessage& request)
{
    // Try to extract command name from first key of Kind-0 BSON
    if (request.isOpMsg() && request.getMsgBody() &&
        !request.getMsgBody()->sections0.empty() &&
        request.getMsgBody()->sections0.front())
    {
        const auto& doc = request.getMsgBody()->sections0.front()->bsonDoc;
        if (doc.size() >= 5)
        {
            size_t off = 4; // skip size
            if (off < doc.size())
            {
                // Skip type byte, read key cstring
                off++;
                size_t start = off;
                while (off < doc.size() && doc[off] != 0x00)
                    off++;
                if (off < doc.size())
                {
                    return std::string(
                        reinterpret_cast<const char*>(&doc[start]),
                        off - start);
                }
            }
        }
    }
    return "hello";
}

string
CDocumentProtocolHandler::extractCollectionName(const vector<uint8_t>& buffer,
                                                ssize_t bytesRead)
{
    // Default collection name
    string collectionName = "test";

    try
    {
        // Handle OP_MSG (opCode 2013) - same parsing as in
        // processDocumentMessage
        if (bytesRead >= 21)
        {
            // Skip header (16 bytes) + flagBits (4 bytes) + kind (1 byte)
            size_t offset = 21;

            // Parse BSON document to extract collection name
            if (offset + 4 < static_cast<size_t>(bytesRead))
            {
                int32_t docSize;
                std::memcpy(&docSize, buffer.data() + offset, 4);
                offset += 4;

                // Parse BSON fields to find collection name from various
                // command formats
                while (offset < static_cast<size_t>(bytesRead) &&
                       offset < static_cast<size_t>(21 + docSize - 1))
                {
                    if (offset >= static_cast<size_t>(bytesRead))
                        break;

                    uint8_t fieldType = buffer[offset++];
                    if (fieldType == 0x00)
                        break; // End of document

                    // Read field name
                    size_t nameStart = offset;
                    while (offset < static_cast<size_t>(bytesRead) &&
                           buffer[offset] != 0x00)
                        offset++;
                    if (offset >= static_cast<size_t>(bytesRead))
                        break;

                    string fieldName(reinterpret_cast<const char*>(
                                         buffer.data() + nameStart),
                                     offset - nameStart);
                    offset++; // Skip null terminator

                    // Handle different command formats where the field value is
                    // a string containing the collection name This covers:
                    // find, findOne, countDocuments, count,
                    // estimatedDocumentCount, etc.
                    if ((fieldName == "find" || fieldName == "findOne" ||
                         fieldName == "count" ||
                         fieldName == "countDocuments" ||
                         fieldName == "estimatedDocumentCount") &&
                        fieldType == 0x02)
                    {
                        // String type - this is the collection name
                        if (offset + 4 < static_cast<size_t>(bytesRead))
                        {
                            int32_t strLen;
                            std::memcpy(&strLen, buffer.data() + offset, 4);
                            offset += 4;

                            if (strLen > 0 &&
                                offset + strLen - 1 <
                                    static_cast<size_t>(bytesRead))
                            {
                                collectionName =
                                    string(reinterpret_cast<const char*>(
                                               buffer.data() + offset),
                                           strLen - 1);
                                break;
                            }
                        }
                    }
                    else if (fieldName == "collection" && fieldType == 0x02)
                    {
                        // Collection field - this is the collection name
                        if (offset + 4 < static_cast<size_t>(bytesRead))
                        {
                            int32_t strLen;
                            std::memcpy(&strLen, buffer.data() + offset, 4);
                            offset += 4;

                            if (strLen > 0 &&
                                offset + strLen - 1 <
                                    static_cast<size_t>(bytesRead))
                            {
                                collectionName =
                                    string(reinterpret_cast<const char*>(
                                               buffer.data() + offset),
                                           strLen - 1);
                                break;
                            }
                        }
                    }
                    else
                    {
                        // Skip field value based on type
                        switch (fieldType)
                        {
                        case 0x01:
                            offset += 8;
                            break; // double
                        case 0x02: // string
                            if (offset + 4 < static_cast<size_t>(bytesRead))
                            {
                                int32_t strLen;
                                std::memcpy(&strLen, buffer.data() + offset, 4);
                                offset += 4 + strLen;
                            }
                            break;
                        case 0x08:
                            offset += 1;
                            break; // boolean
                        case 0x10:
                            offset += 4;
                            break; // int32
                        case 0x12:
                            offset += 8;
                            break; // int64
                        default:
                            // For other types, skip to end to avoid infinite
                            // loop
                            break;
                        }
                    }
                }
            }
        }
    }
    catch (...)
    {
        // If parsing fails, return default
    }

    return collectionName;
}

vector<uint8_t>
CDocumentProtocolHandler::createErrorBsonDocument(int errorCode,
                                                  const string& errorMessage)
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 0.0);
    bson.addInt32("code", errorCode);
    bson.addString("errmsg", errorMessage);
    bson.endDocument();

    // Create proper OP_MSG wire protocol response
    vector<uint8_t> response;
    vector<uint8_t> bsonDoc = bson.getDocument();
    int32_t bodySize = 4 + 1 + static_cast<int32_t>(bsonDoc.size());
    int32_t messageSize = 16 + bodySize;

    response.resize(messageSize);
    size_t offset = 0;

    // Header
    std::memcpy(response.data() + offset, &messageSize, 4);
    offset += 4;
    int32_t requestID = 1;
    std::memcpy(response.data() + offset, &requestID, 4);
    offset += 4;
    std::memcpy(response.data() + offset, &requestID, 4);
    offset += 4;
    int32_t opCode = 2013;
    std::memcpy(response.data() + offset, &opCode, 4);
    offset += 4;

    // Body
    uint32_t flagBits = 0;
    std::memcpy(response.data() + offset, &flagBits, 4);
    offset += 4;
    uint8_t kind = 0;
    response[offset++] = kind;
    std::memcpy(response.data() + offset, bsonDoc.data(), bsonDoc.size());

    return response;
}

} // namespace FauxDB

// Command Handler Implementations
namespace FauxDB
{

CHelloCommandHandler::CHelloCommandHandler()
{
}

CHelloCommandHandler::~CHelloCommandHandler()
{
}

std::unique_ptr<CDocumentWireMessage>
CHelloCommandHandler::handleCommand(const CDocumentWireMessage& request)
{
    (void)request;
    auto response = std::make_unique<CDocumentWireMessage>();
    /* Hello response */
    return response;
}

std::vector<std::string> CHelloCommandHandler::getSupportedCommands() const
{
    return {"hello"};
}

bool CHelloCommandHandler::isCommandSupported(const std::string& command) const
{
    return command == "hello";
}

std::vector<uint8_t> CHelloCommandHandler::createHelloResponseDocument()
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addString("ok", "1.0");
    bson.addString("msg", "hello");
    bson.endDocument();
    return bson.getDocument();
}

CBuildInfoCommandHandler::CBuildInfoCommandHandler()
{
}

CBuildInfoCommandHandler::~CBuildInfoCommandHandler()
{
}

std::unique_ptr<CDocumentWireMessage>
CBuildInfoCommandHandler::handleCommand(const CDocumentWireMessage& request)
{
    (void)request;
    auto response = std::make_unique<CDocumentWireMessage>();
    /* BuildInfo response */
    return response;
}

std::vector<std::string> CBuildInfoCommandHandler::getSupportedCommands() const
{
    return {"buildInfo"};
}

bool CBuildInfoCommandHandler::isCommandSupported(
    const std::string& command) const
{
    return command == "buildInfo";
}

std::vector<uint8_t> CBuildInfoCommandHandler::createBuildInfoResponseDocument()
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addString("version", "4.4.0");
    bson.addString("gitVersion", "abcdef1234567890");
    bson.endDocument();
    return bson.getDocument();
}

CIsMasterCommandHandler::CIsMasterCommandHandler()
{
}

CIsMasterCommandHandler::~CIsMasterCommandHandler()
{
}

std::unique_ptr<CDocumentWireMessage>
CIsMasterCommandHandler::handleCommand(const CDocumentWireMessage& request)
{
    (void)request;
    auto response = std::make_unique<CDocumentWireMessage>();
    /* IsMaster response */
    return response;
}

std::vector<std::string> CIsMasterCommandHandler::getSupportedCommands() const
{
    return {"isMaster"};
}

bool CIsMasterCommandHandler::isCommandSupported(
    const std::string& command) const
{
    return command == "isMaster";
}

std::vector<uint8_t> CIsMasterCommandHandler::createIsMasterResponseDocument()
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addBool("ismaster", true);
    bson.addString("msg", "isMaster");
    bson.endDocument();
    return bson.getDocument();
}

vector<CBsonType> CDocumentProtocolHandler::extractDocumentsFromInsert(
    const vector<uint8_t>& buffer, ssize_t bytesRead)
{
    vector<CBsonType> documents;

    /* Simple implementation - extract documents from insert command BSON */
    try
    {
        if (bytesRead >= 21)
        {

            /* For now, create a simple document with test data */
            CBsonType doc;
            doc.initialize();
            doc.beginDocument();
            doc.addString("_id", "auto_generated_id");
            doc.addString("data", "inserted_data");
            doc.addInt32("timestamp", static_cast<int32_t>(time(nullptr)));
            doc.endDocument();
            documents.push_back(doc);
        }
    }
    catch (const std::exception& e)
    {
    }

    return documents;
}

vector<UpdateOperation>
CDocumentProtocolHandler::extractUpdateOperations(const vector<uint8_t>& buffer,
                                                  ssize_t bytesRead)
{
    vector<UpdateOperation> operations;

    /* Simple implementation - extract update operations from update command
     * BSON */
    try
    {
        if (bytesRead >= 21)
        {
            UpdateOperation op;

            op.filterJson = "{\"_id\": \"test_id\"}";
            op.updateJson = "{\"$set\": {\"updated\": true}}";

            operations.push_back(op);
        }
    }
    catch (const std::exception& e)
    {
    }

    return operations;
}

vector<DeleteOperation>
CDocumentProtocolHandler::extractDeleteOperations(const vector<uint8_t>& buffer,
                                                  ssize_t bytesRead)
{
    vector<DeleteOperation> operations;

    /* Simple implementation - extract delete operations from delete command
     * BSON */
    try
    {
        if (bytesRead >= 21)
        {
            DeleteOperation op;

            op.filterJson = "{\"_id\": \"test_id\"}";

            operations.push_back(op);
        }
    }
    catch (const std::exception& e)
    {
    }

    return operations;
}

string
CDocumentProtocolHandler::convertBsonToInsertSQL(const string& collectionName,
                                                 const CBsonType& doc)
{

    string sql = "INSERT INTO " + collectionName + " (data) VALUES ('";

    /* Simple implementation - convert to JSON-like string */
    sql += "{\"_id\": \"auto_generated\", \"data\": \"inserted_data\", "
           "\"timestamp\": ";
    sql += std::to_string(time(nullptr));
    sql += "}')";

    return sql;
}

string
CDocumentProtocolHandler::convertUpdateToSQL(const string& collectionName,
                                             const string& filterJson,
                                             const string& updateJson)
{

    /* For now, use a simple UPDATE with JSON column */
    string sql = "UPDATE " + collectionName +
                 " SET data = '{\"updated\": true, \"timestamp\": ";
    sql += std::to_string(time(nullptr));
    sql += "}' WHERE data::json->>'_id' = 'test_id'";

    return sql;
}

string
CDocumentProtocolHandler::convertDeleteToSQL(const string& collectionName,
                                             const string& filterJson)
{
    /* Convert MongoDB delete to SQL DELETE statement */
    /* For now, use a simple DELETE with JSON column */
    string sql = "DELETE FROM " + collectionName +
                 " WHERE data::json->>'_id' = 'test_id'";

    return sql;
}

} // namespace FauxDB
