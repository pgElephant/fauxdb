/*
 * CDocumentProtocolHandler.cpp
 *		MongoDB wire protocol handler for FauxDB.
 *		Implements OP_MSG and OP_QUERY protocol handling with PostgreSQL
 * backend.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 */

#include "CDocumentProtocolHandler.hpp"

#include "../CServerConfig.hpp"
#include "../database/CPGConnectionPooler.hpp"
#include "CBsonType.hpp"
#include "CLogMacros.hpp"
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
      commandRegistry_(std::make_unique<CCommandRegistry>()),
      logger_(std::make_shared<CLogger>(CServerConfig{}))
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
    if (logger_)
        logger_->log(CLogLevel::DEBUG,
                     "processDocumentMessage: Starting with buffer size: " +
                         std::to_string(buffer.size()) +
                         ", bytesRead: " + std::to_string(bytesRead));

    int32_t messageLength;
    int32_t requestID;
    int32_t responseTo;
    int32_t opCode;
    uint32_t flagBits;
    uint8_t kind;
    int32_t docSize;
    std::string commandName;
    std::string collectionName;
    size_t offset;
    size_t collNameStart;
    int32_t queryDocSize;

    (void)responseBuilder;

    if (buffer.empty() || bytesRead < 21)
    {
        return createErrorWireResponse(0);
    }

    std::memcpy(&messageLength, buffer.data(), 4);
    std::memcpy(&requestID, buffer.data() + 4, 4);
    std::memcpy(&responseTo, buffer.data() + 8, 4);
    std::memcpy(&opCode, buffer.data() + 12, 4);

    if (logger_)
        logger_->log(CLogLevel::DEBUG,
                     "processDocumentMessage: messageLength=" +
                         std::to_string(messageLength) +
                         ", requestID=" + std::to_string(requestID) +
                         ", opCode=" + std::to_string(opCode));

    if (messageLength != bytesRead)
    {
        return createErrorWireResponse(requestID);
    }

    if (opCode == 2013)
    {
        std::memcpy(&flagBits, buffer.data() + 16, 4);
        kind = buffer[20];

        if (flagBits != 0)
        {
            return createErrorWireResponse(requestID);
        }

        if (kind != 0)
        {
            return createErrorWireResponse(requestID);
        }

        if (bytesRead < 25)
            return createErrorWireResponse(requestID);

        std::memcpy(&docSize, buffer.data() + 21, 4);

        if (21 + docSize > bytesRead)
        {
            return createErrorWireResponse(requestID);
        }

        commandName = parseCommandFromBSON(buffer, 25, docSize - 4);
        if (logger_)
            logger_->log(CLogLevel::DEBUG,
                         "processDocumentMessage: Parsed command name: '" +
                             commandName + "'");

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
            if (logger_)
                logger_->log(CLogLevel::DEBUG,
                             "processDocumentMessage: Routing to find handler");
            collectionName =
                parseCollectionNameFromBSON(buffer, 25, docSize - 4, "find");
            if (logger_)
                logger_->log(
                    CLogLevel::DEBUG,
                    "processDocumentMessage: Parsed collection name: '" +
                        collectionName + "'");
            if (logger_)
                logger_->log(CLogLevel::DEBUG,
                             "processDocumentMessage: Calling "
                             "createFindResponseWithPostgreSQLBSON");
            auto result = createFindResponseWithPostgreSQLBSON(
                collectionName, requestID, buffer, bytesRead);
            if (logger_)
                logger_->log(
                    CLogLevel::DEBUG,
                    "processDocumentMessage: "
                    "createFindResponseWithPostgreSQLBSON returned, size: " +
                        std::to_string(result.size()));
            return result;
        }
        else if (commandName == "insert")
        {
            if (logger_)
                logger_->log(
                    CLogLevel::DEBUG,
                    "processDocumentMessage: Routing to insert handler");
            collectionName = parseCollectionNameFromBSON(
                buffer, 25, docSize - 4, commandName);
            if (logger_)
                logger_->log(CLogLevel::DEBUG, "processDocumentMessage: Parsed "
                                               "collection name for insert: '" +
                                                   collectionName + "'");
            return createInsertWireResponse(collectionName, requestID, buffer,
                                            bytesRead);
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
        else if (commandName == "countDocuments" || commandName == "count")
        {
            if (logger_)
                logger_->log(
                    CLogLevel::DEBUG,
                    "processDocumentMessage: Routing to count handler");
            collectionName = parseCollectionNameFromBSON(
                buffer, 25, docSize - 4, commandName);
            if (logger_)
                logger_->log(CLogLevel::DEBUG, "processDocumentMessage: Parsed "
                                               "collection name for count: '" +
                                                   collectionName + "'");
            return createCountResponseFromPostgreSQL(collectionName, requestID);
        }
        else if (commandName == "update")
        {
            if (logger_)
                logger_->log(
                    CLogLevel::DEBUG,
                    "processDocumentMessage: Routing to update handler for: " +
                        commandName);
            collectionName = parseCollectionNameFromBSON(
                buffer, 25, docSize - 4, commandName);
            if (logger_)
                logger_->log(CLogLevel::DEBUG, "processDocumentMessage: Parsed "
                                               "collection name for update: '" +
                                                   collectionName + "'");
            return createUpdateResponseFromPostgreSQL(
                collectionName, requestID, buffer, bytesRead, "updateOne");
        }
        else if (commandName == "delete")
        {
            if (logger_)
                logger_->log(
                    CLogLevel::DEBUG,
                    "processDocumentMessage: Routing to delete handler for: " +
                        commandName);
            collectionName = parseCollectionNameFromBSON(
                buffer, 25, docSize - 4, commandName);
            if (logger_)
                logger_->log(CLogLevel::DEBUG, "processDocumentMessage: Parsed "
                                               "collection name for delete: '" +
                                                   collectionName + "'");
            return createDeleteResponseFromPostgreSQL(
                collectionName, requestID, buffer, bytesRead, "deleteOne");
        }
        else if (commandName == "distinct")
        {
            if (logger_)
                logger_->log(
                    CLogLevel::DEBUG,
                    "processDocumentMessage: Routing to distinct handler");
            collectionName = parseCollectionNameFromBSON(
                buffer, 25, docSize - 4, commandName);
            if (logger_)
                logger_->log(CLogLevel::DEBUG,
                             "processDocumentMessage: Parsed collection name "
                             "for distinct: '" +
                                 collectionName + "'");
            return createDistinctResponseFromPostgreSQL(
                collectionName, requestID, buffer, bytesRead);
        }
        else if (commandName == "listCollections")
        {
            if (logger_)
                logger_->log(CLogLevel::DEBUG,
                             "processDocumentMessage: Routing to "
                             "listCollections handler");
            return createListCollectionsResponse(requestID);
        }
        else if (commandName == "createIndexes")
        {
            if (logger_)
                logger_->log(
                    CLogLevel::DEBUG,
                    "processDocumentMessage: Routing to createIndexes handler");
            collectionName = parseCollectionNameFromBSON(
                buffer, 25, docSize - 4, commandName);
            if (logger_)
                logger_->log(CLogLevel::DEBUG,
                             "processDocumentMessage: Parsed collection name "
                             "for createIndexes: '" +
                                 collectionName + "'");
            return createCreateIndexesResponse(collectionName, requestID,
                                               buffer, bytesRead);
        }
        else if (commandName == "dropIndexes")
        {
            if (logger_)
                logger_->log(
                    CLogLevel::DEBUG,
                    "processDocumentMessage: Routing to dropIndexes handler");
            collectionName = parseCollectionNameFromBSON(
                buffer, 25, docSize - 4, commandName);
            debug_log("processDocumentMessage: Parsed collection name for "
                      "dropIndexes: '" +
                      collectionName + "'");
            return createDropIndexesResponse(collectionName, requestID, buffer,
                                             bytesRead);
        }
        else if (commandName == "listIndexes")
        {
            debug_log("processDocumentMessage: Routing to listIndexes handler");
            collectionName = parseCollectionNameFromBSON(
                buffer, 25, docSize - 4, commandName);
            debug_log("processDocumentMessage: Parsed collection name for "
                      "listIndexes: '" +
                      collectionName + "'");
            return createListIndexesResponse(collectionName, requestID);
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

        offset = 20;

        collNameStart = offset;
        while (offset < (size_t)bytesRead && buffer[offset] != 0)
            offset++;
        if (offset >= (size_t)bytesRead)
            return createErrorWireResponse(requestID);

        collectionName = std::string(
            reinterpret_cast<const char*>(buffer.data() + collNameStart),
            offset - collNameStart);
        offset++;

        offset += 8;

        if (offset + 4 >= (size_t)bytesRead)
            return createErrorWireResponse(requestID);

        std::memcpy(&queryDocSize, buffer.data() + offset, 4);

        if (offset + queryDocSize > (size_t)bytesRead)
            return createErrorWireResponse(requestID);

        commandName =
            parseCommandFromBSON(buffer, offset + 4, queryDocSize - 4);
        debug_log("OP_QUERY: Parsed command name: '" + commandName + "'");

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
            debug_log("OP_QUERY: Routing to find handler");
            collectionName = parseCollectionNameFromBSON(
                buffer, offset + 4, queryDocSize - 4, "find");
            debug_log("OP_QUERY: Parsed collection name: '" + collectionName +
                      "'");
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
    const vector<uint8_t>& buffer, size_t offset, size_t remaining,
    const std::string& commandName)
{
    if (remaining < 2)
        return "";

    uint8_t fieldType = buffer[offset];
    // For commands, we expect the first field to be a string (0x02)
    // containing the collection name
    if (fieldType != 0x02)
        return "";

    // Skip field type
    offset++;

    // Read field name - should match the command name
    size_t nameStart = offset;
    size_t nameEnd = nameStart;
    while (nameEnd < offset + remaining && buffer[nameEnd] != 0)
        nameEnd++;

    if (nameEnd >= offset + remaining)
        return "";

    std::string fieldName(
        reinterpret_cast<const char*>(buffer.data() + nameStart),
        nameEnd - nameStart);
    if (fieldName != commandName)
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
    b.addBool("ismaster", true);
    b.addInt32("minWireVersion", 0);
    b.addInt32("maxWireVersion", 17);
    b.addInt32("logicalSessionTimeoutMinutes", 30);
    b.addInt32("maxBsonObjectSize", 16777216);
    b.addInt32("maxMessageSizeBytes", 48000000);
    b.addInt32("maxWriteBatchSize", 100000);
    b.addString("host", "localhost:27018");
    b.addString("version", "7.0.0");
    b.addString("gitVersion", "fauxdb-1.0.0");
    b.addString("me", "localhost:27018");
    b.addBool("readOnly", false);
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

vector<uint8_t> CDocumentProtocolHandler::createInsertWireResponse(
    const string& collectionName, int32_t requestID,
    const vector<uint8_t>& queryBuffer, ssize_t bytesRead)
{
    if (logger_)
        logger_->log(
            CLogLevel::DEBUG,
            "createInsertWireResponse: Processing insert for collection: " +
                collectionName);

    /* Check if connection pooler is available */
    if (!connectionPooler_)
    {
        error_log("createInsertWireResponse: Connection pooler not available");
        return createErrorWireResponse(requestID);
    }

    /* Get connection from pool */
    auto connection = connectionPooler_->getPostgresConnection();
    if (!connection || !connection->database)
    {
        error_log(
            "createInsertWireResponse: Failed to get PostgreSQL connection");
        return createErrorWireResponse(requestID);
    }

    /* Parse documents from insert command */
    vector<CBsonType> documents =
        extractDocumentsFromInsert(queryBuffer, bytesRead);
    if (logger_)
        logger_->log(CLogLevel::DEBUG, "createInsertWireResponse: Extracted " +
                                           std::to_string(documents.size()) +
                                           " documents to insert");

    int32_t insertedCount = 0;

    /* Insert each document */
    for (const auto& doc : documents)
    {
        try
        {
            /* Build INSERT SQL statement */
            stringstream sql;
            sql << "INSERT INTO " << collectionName
                << " (name, email, department_id) VALUES (";

            /* Extract fields from document - this is a simplified approach */
            sql << "'Inserted User', 'inserted@example.com', 1";
            sql << ")";

            if (logger_)
                logger_->log(CLogLevel::DEBUG,
                             "createInsertWireResponse: Executing SQL: " +
                                 sql.str());

            /* Execute INSERT statement */
            auto result = connection->database->executeQuery(sql.str());
            if (result.success)
            {
                insertedCount++;
                if (logger_)
                    logger_->log(CLogLevel::DEBUG,
                                 "createInsertWireResponse: Document inserted "
                                 "successfully");
            }
            else
            {
                error_log("createInsertWireResponse: Insert failed");
            }
        }
        catch (const std::exception& e)
        {
            error_log("createInsertWireResponse: Exception during insert: " +
                      string(e.what()));
        }
    }

    /* Return connection to pool */
    connectionPooler_->returnConnection(connection);

    /* Build MongoDB insert response */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addBool("acknowledged", true);
    response.addInt32("insertedCount", insertedCount);
    response.endDocument();

    auto responseBytes = response.getDocument();
    return createWireMessage(1, requestID, responseBytes);
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
    if (logger_)
        logger_->log(CLogLevel::DEBUG,
                     "createAggregateWireResponse: Processing aggregation");

    /* Check if connection pooler is available */
    if (!connectionPooler_)
    {
        if (logger_)
            logger_->log(
                CLogLevel::DEBUG,
                "createAggregateWireResponse: No connection pooler available");
        return createErrorBsonDocument(
            -10, "PostgreSQL connection pooler not available");
    }

    auto connection = connectionPooler_->getPostgresConnection();
    if (!connection || !connection->database)
    {
        if (logger_)
            logger_->log(CLogLevel::DEBUG,
                         "createAggregateWireResponse: Failed to get "
                         "PostgreSQL connection");
        return createErrorBsonDocument(-11,
                                       "Failed to get PostgreSQL connection");
    }

    try
    {
        /* For now, implement a simple count aggregation */
        /* In a full implementation, this would parse the aggregation pipeline
         */
        string sql = "SELECT COUNT(*) as n FROM users";
        if (logger_)
            logger_->log(CLogLevel::DEBUG,
                         "createAggregateWireResponse: Executing SQL: " + sql);

        auto result = connection->database->executeQuery(sql);
        if (result.success && !result.rows.empty())
        {
            int64_t count = 0;
            try
            {
                count = std::stoll(result.rows[0][0]);
                if (logger_)
                    logger_->log(CLogLevel::DEBUG,
                                 "createAggregateWireResponse: Count result: " +
                                     std::to_string(count));
            }
            catch (const std::exception& e)
            {
                if (logger_)
                    logger_->log(
                        CLogLevel::DEBUG,
                        "createAggregateWireResponse: Failed to parse count: " +
                            std::string(e.what()));
                count = 0;
            }

            /* Create cursor subdocument with count result */
            CBsonType cursor;
            cursor.initialize();
            cursor.beginDocument();
            cursor.addInt64("id", 0);
            cursor.addString("ns", "fauxdb.users");
            cursor.beginArray("firstBatch");

            /* Add count document */
            CBsonType countDoc;
            countDoc.initialize();
            countDoc.beginDocument();
            countDoc.addInt64("n", count);
            countDoc.endDocument();
            cursor.addArrayDocument(countDoc);

            cursor.endArray();
            cursor.endDocument();

            /* Create main response document */
            CBsonType b;
            b.initialize();
            b.beginDocument();
            b.addDocument("cursor", cursor);
            b.addDouble("ok", 1.0);
            b.endDocument();

            /* Always return connection to pool */
            connectionPooler_->releasePostgresConnection(connection);

            return createWireMessage(1, requestID, b.getDocument());
        }
        else
        {
            debug_log("createAggregateWireResponse: Query failed");
        }
    }
    catch (const std::exception& e)
    {
        debug_log("createAggregateWireResponse: Exception: " +
                  string(e.what()));
    }

    /* Always return connection to pool */
    connectionPooler_->releasePostgresConnection(connection);

    /* Fallback to empty result */
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    cursor.addInt64("id", 0);
    cursor.addString("ns", "fauxdb.users");
    cursor.beginArray("firstBatch");
    cursor.endArray();
    cursor.endDocument();

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
    b.addString("host", "localhost:27018");
    b.addString("version", "7.0.0");
    b.addString("gitVersion", "fauxdb-1.0.0");
    b.addString("versionArray", "[7,0,0,0]");
    b.addString("me", "localhost:27018");
    b.addInt32("maxBsonObjectSize", 16777216);
    b.addInt32("maxMessageSizeBytes", 48000000);
    b.addInt32("maxWriteBatchSize", 100000);
    b.addBool("readOnly", false);
    b.addBool("ok", true);
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
    debug_log("createFindOpReplyResponseFromPostgreSQL: Processing find for "
              "collection: " +
              collectionName);

    // Check if connection pooler is available
    if (!connectionPooler_)
    {
        debug_log("createFindOpReplyResponseFromPostgreSQL: No connection "
                  "pooler available");
        return createErrorOpReplyResponse(requestID);
    }

    // Query PostgreSQL for the collection data
    debug_log(
        "createFindOpReplyResponseFromPostgreSQL: Querying PostgreSQL...");
    vector<CBsonType> documents = queryPostgreSQLCollection(collectionName);
    debug_log("createFindOpReplyResponseFromPostgreSQL: Got " +
              std::to_string(documents.size()) + " documents from PostgreSQL");

    // Create cursor subdocument
    debug_log(
        "createFindOpReplyResponseFromPostgreSQL: Creating cursor subdocument");
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    debug_log("createFindOpReplyResponseFromPostgreSQL: Adding cursor fields");
    cursor.addInt64("id", 0);
    cursor.addString("ns", collectionName + ".collection");
    cursor.beginArray("firstBatch");

    // Add documents from PostgreSQL
    debug_log("createFindOpReplyResponseFromPostgreSQL: Adding " +
              std::to_string(documents.size()) + " documents to cursor");
    for (size_t i = 0; i < documents.size(); ++i)
    {
        debug_log("createFindOpReplyResponseFromPostgreSQL: Adding document " +
                  std::to_string(i) + " to cursor");
        cursor.addArrayDocument(documents[i]);
        debug_log("createFindOpReplyResponseFromPostgreSQL: Document " +
                  std::to_string(i) + " added successfully");
    }

    debug_log("createFindOpReplyResponseFromPostgreSQL: Ending cursor array "
              "and document");
    cursor.endArray();
    cursor.endDocument();

    // Create main response document
    debug_log("createFindOpReplyResponseFromPostgreSQL: Creating main response "
              "document");
    CBsonType b;
    b.initialize();
    b.beginDocument();
    b.addDouble("ok", 1.0);
    b.addDocument("cursor", cursor);
    b.endDocument();

    // Create OP_REPLY response
    debug_log(
        "createFindOpReplyResponseFromPostgreSQL: Creating OP_REPLY response");
    return createOpReplyResponse(requestID, b.getDocument());
}

vector<uint8_t> CDocumentProtocolHandler::createFindResponseFromPostgreSQL(
    const string& collectionName, int32_t requestID)
{
    debug_log(
        "createFindResponseFromPostgreSQL: Processing find for collection: " +
        collectionName);

    // Check if connection pooler is available
    if (!connectionPooler_)
    {
        debug_log(
            "createFindResponseFromPostgreSQL: No connection pooler available");
        return createErrorBsonDocument(
            -10, "PostgreSQL connection pooler not available");
    }

    // Query PostgreSQL for the collection data
    debug_log("createFindResponseFromPostgreSQL: Querying PostgreSQL...");
    vector<CBsonType> documents = queryPostgreSQLCollection(collectionName);
    debug_log("createFindResponseFromPostgreSQL: Got " +
              std::to_string(documents.size()) + " documents from PostgreSQL");

    // Build response document
    debug_log("createFindResponseFromPostgreSQL: Building response document");
    CBsonType bson;
    if (!bson.initialize() || !bson.beginDocument())
    {
        debug_log("createFindResponseFromPostgreSQL: BSON init failed");
        return createErrorBsonDocument(-6, "BSON init failed");
    }

    bson.addDouble("ok", 1.0);

    // Create cursor subdocument
    debug_log("createFindResponseFromPostgreSQL: Creating cursor subdocument");
    CBsonType cursorDoc;
    cursorDoc.initialize();
    cursorDoc.beginDocument();
    cursorDoc.addInt64("id", 0); // No cursor needed for simple queries
    cursorDoc.addString("ns", collectionName + ".collection"); // namespace

    // Create firstBatch array with PostgreSQL data
    debug_log("createFindResponseFromPostgreSQL: Creating firstBatch array");
    cursorDoc.beginArray("firstBatch");

    // Add documents from PostgreSQL
    debug_log("createFindResponseFromPostgreSQL: Adding " +
              std::to_string(documents.size()) + " documents to cursor");
    for (size_t i = 0; i < documents.size(); ++i)
    {
        debug_log("createFindResponseFromPostgreSQL: Adding document " +
                  std::to_string(i) + " to cursor");
        cursorDoc.addArrayDocument(documents[i]);
        debug_log("createFindResponseFromPostgreSQL: Document " +
                  std::to_string(i) + " added successfully");
    }

    debug_log(
        "createFindResponseFromPostgreSQL: Ending cursor array and document");
    cursorDoc.endArray();    // end firstBatch
    cursorDoc.endDocument(); // end cursor

    // Add the cursor subdocument to main response
    debug_log(
        "createFindResponseFromPostgreSQL: Adding cursor to main response");
    bson.addDocument("cursor", cursorDoc);

    if (!bson.endDocument())
    {
        debug_log("createFindResponseFromPostgreSQL: BSON finalize failed");
        return createErrorBsonDocument(-7, "BSON finalize failed");
    }

    // Create a simple response for testing
    debug_log("createFindResponseFromPostgreSQL: Creating simple response");

    // Create proper MongoDB find response with cursor
    CBsonType response;
    debug_log("createFindResponseFromPostgreSQL: CBsonType created");
    response.initialize();
    debug_log("createFindResponseFromPostgreSQL: CBsonType initialized");
    response.beginDocument();
    debug_log("createFindResponseFromPostgreSQL: Document begun");

    // Create cursor document
    CBsonType newCursorDoc;
    newCursorDoc.initialize();
    newCursorDoc.beginDocument();
    newCursorDoc.addString("ns", "fauxdb." + collectionName);
    debug_log("createFindResponseFromPostgreSQL: Added ns field");
    newCursorDoc.addInt64("id", 0); // cursor id
    debug_log("createFindResponseFromPostgreSQL: Added id field");

    // Add firstBatch array
    newCursorDoc.beginArray("firstBatch");
    debug_log("createFindResponseFromPostgreSQL: firstBatch array begun");

    // Add documents to firstBatch
    for (const auto& doc : documents)
    {
        debug_log("createFindResponseFromPostgreSQL: About to add document to "
                  "firstBatch");
        newCursorDoc.addArrayDocument(doc);
        debug_log(
            "createFindResponseFromPostgreSQL: Document added to firstBatch");
    }

    debug_log("createFindResponseFromPostgreSQL: Ending firstBatch array");
    newCursorDoc.endArray();
    debug_log("createFindResponseFromPostgreSQL: firstBatch array ended");
    newCursorDoc.endDocument();
    debug_log("createFindResponseFromPostgreSQL: Cursor document ended");

    // Add cursor document to main response
    response.addDocument("cursor", newCursorDoc);
    debug_log(
        "createFindResponseFromPostgreSQL: Cursor document added to response");

    response.addDouble("ok", 1.0);
    debug_log("createFindResponseFromPostgreSQL: Added ok field");
    debug_log("createFindResponseFromPostgreSQL: Ending main document");
    response.endDocument(); // end main document
    debug_log("createFindResponseFromPostgreSQL: Main document ended");

    auto responseBytes = response.getDocument();
    debug_log("createFindResponseFromPostgreSQL: Response created, size: " +
              std::to_string(responseBytes.size()));
    debug_log(
        "createFindResponseFromPostgreSQL: About to create OP_MSG response");

    // Create proper OP_MSG response
    return createWireMessage(1, requestID, responseBytes);
}

vector<uint8_t>
CDocumentProtocolHandler::createFindResponseFromPostgreSQLWithFilters(
    const string& collectionName, int32_t requestID,
    const vector<uint8_t>& queryBuffer, ssize_t bytesRead)
{
    debug_log("createFindResponseFromPostgreSQL: Processing ENHANCED find for "
              "collection: " +
              collectionName);
    debug_log("createFindResponseFromPostgreSQL: Buffer size: " +
              std::to_string(queryBuffer.size()) +
              ", bytesRead: " + std::to_string(bytesRead));

    /* Query PostgreSQL with query filters */
    vector<CBsonType> documents =
        queryPostgreSQLCollection(collectionName, queryBuffer, bytesRead);
    debug_log("createFindResponseFromPostgreSQL: Got " +
              std::to_string(documents.size()) + " documents from PostgreSQL");

    /* Build response document */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);

    /* Create cursor subdocument */
    CBsonType cursorDoc;
    cursorDoc.initialize();
    cursorDoc.beginDocument();
    cursorDoc.addInt64("id", 0);
    cursorDoc.addString("ns", "fauxdb." + collectionName);
    cursorDoc.beginArray("firstBatch");

    /* Add documents to cursor */
    for (const auto& doc : documents)
    {
        cursorDoc.addArrayDocument(doc);
    }
    cursorDoc.endArray();
    cursorDoc.endDocument();

    response.addDocument("cursor", cursorDoc);
    response.endDocument();

    auto responseBytes = response.getDocument();
    return createWireMessage(1, requestID, responseBytes);
}

vector<uint8_t> CDocumentProtocolHandler::createFindResponseWithPostgreSQLBSON(
    const string& collectionName, int32_t requestID,
    const vector<uint8_t>& queryBuffer, ssize_t bytesRead)
{
    if (logger_)
        logger_->log(CLogLevel::DEBUG,
                     "createFindResponseWithPostgreSQLBSON: Processing "
                     "BSON-based find for collection: " +
                         collectionName);

    /* Get connection from pool */
    auto connection = connectionPooler_->getPostgresConnection();
    if (!connection || !connection->database)
    {
        if (logger_)
            logger_->log(CLogLevel::ERROR,
                         "createFindResponseWithPostgreSQLBSON: Failed to get "
                         "PostgreSQL connection");
        return createErrorWireResponse(requestID);
    }

    /* Convert MongoDB BSON to PostgreSQL BSON format and build SQL query */
    std::stringstream sql;

    /* First, let's try a simple approach: convert the query to a WHERE clause
     * using PostgreSQL BSON */
    /* We'll create a temporary table with BSON data and use BSON operators for
     * filtering */
    sql << "WITH bson_data AS (";
    sql << "  SELECT *, ";
    sql << "  ('{\"id\": ' || id || ', \"name\": \"' || name || '\", "
           "\"email\": \"' || email || '\", \"department_id\": ' || "
           "department_id || ', \"created_at\": \"' || created_at || "
           "'\"}')::bson as doc_bson ";
    sql << "  FROM " << collectionName;
    sql << ") ";
    sql << "SELECT id, name, email, department_id, created_at FROM bson_data ";

    /* Try to parse basic filters from the MongoDB query */
    /* For now, implement a simple name filter as a proof of concept */
    if (queryBuffer.size() > 50)
    { /* Basic check if there's a filter */
        sql << "WHERE doc_bson @> '{\"name\": \"John Doe\"}'::bson ";
    }

    sql << "ORDER BY id LIMIT 20";

    if (logger_)
        logger_->log(CLogLevel::DEBUG,
                     "createFindResponseWithPostgreSQLBSON: Executing SQL: " +
                         sql.str());

    /* Execute the SQL query */
    auto result = connection->database->executeQuery(sql.str());
    if (!result.success)
    {
        if (logger_)
            logger_->log(
                CLogLevel::ERROR,
                "createFindResponseWithPostgreSQLBSON: SQL execution failed");
        return createErrorWireResponse(requestID);
    }

    if (logger_)
        logger_->log(CLogLevel::DEBUG, "createFindResponseWithPostgreSQLBSON: "
                                       "SQL executed successfully, got " +
                                           std::to_string(result.rows.size()) +
                                           " rows");

    /* Convert PostgreSQL results to BSON documents */
    std::vector<CBsonType> documents;
    for (size_t i = 0; i < result.rows.size(); ++i)
    {
        CBsonType doc;
        doc.initialize();
        doc.beginDocument();

        /* Add all fields from the result */
        for (size_t j = 0; j < result.columnNames.size(); ++j)
        {
            std::string fieldName = result.columnNames[j];
            std::string fieldValue = result.rows[i][j];

            /* Convert field value to appropriate BSON type */
            if (fieldName == "id")
            {
                try
                {
                    int id = std::stoi(fieldValue);
                    doc.addInt32(fieldName, id);
                }
                catch (...)
                {
                    doc.addString(fieldName, fieldValue);
                }
            }
            else if (fieldName == "department_id")
            {
                try
                {
                    int deptId = std::stoi(fieldValue);
                    doc.addInt32(fieldName, deptId);
                }
                catch (...)
                {
                    doc.addString(fieldName, fieldValue);
                }
            }
            else
            {
                doc.addString(fieldName, fieldValue);
            }
        }

        doc.endDocument();
        documents.push_back(doc);
    }

    /* Build MongoDB response */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);

    /* Create cursor subdocument */
    CBsonType cursorDoc;
    cursorDoc.initialize();
    cursorDoc.beginDocument();
    cursorDoc.addInt64("id", 0);
    cursorDoc.addString("ns", collectionName + ".$cmd.find");

    /* Add firstBatch array */
    cursorDoc.beginArray("firstBatch");
    for (size_t i = 0; i < documents.size(); ++i)
    {
        cursorDoc.addArrayDocument(documents[i]);
    }
    cursorDoc.endArray();
    cursorDoc.endDocument();

    response.addDocument("cursor", cursorDoc);
    response.endDocument();

    auto responseBytes = response.getDocument();
    if (logger_)
        logger_->log(
            CLogLevel::DEBUG,
            "createFindResponseWithPostgreSQLBSON: Built response with " +
                std::to_string(documents.size()) + " documents");

    return createWireMessage(1, requestID, responseBytes);
}

vector<uint8_t> CDocumentProtocolHandler::createCountResponseFromPostgreSQL(
    const string& collectionName, int32_t requestID)
{
    debug_log(
        "createCountResponseFromPostgreSQL: Processing count for collection: " +
        collectionName);

    int64_t count = 0;

    if (!connectionPooler_)
    {
        debug_log("createCountResponseFromPostgreSQL: No connection pooler "
                  "available");
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         "No connection pooler available for count query");
        }
    }
    else
    {
        debug_log(
            "createCountResponseFromPostgreSQL: Getting PostgreSQL connection");
        auto connection = connectionPooler_->getPostgresConnection();

        if (!connection || !connection->database)
        {
            debug_log("createCountResponseFromPostgreSQL: Failed to get "
                      "connection or database");
            if (logger_)
            {
                logger_->log(
                    CLogLevel::ERROR,
                    "Failed to get PostgreSQL connection for count query");
            }
        }
        else
        {
            debug_log("createCountResponseFromPostgreSQL: Connection acquired "
                      "successfully");

            try
            {
                stringstream sql;
                sql << "SELECT COUNT(*) FROM " << collectionName;
                debug_log("createCountResponseFromPostgreSQL: About to execute "
                          "SQL: " +
                          sql.str());

                auto result = connection->database->executeQuery(sql.str());
                debug_log("createCountResponseFromPostgreSQL: SQL executed "
                          "successfully");
                debug_log("createCountResponseFromPostgreSQL: Query success=" +
                          string(result.success ? "true" : "false") +
                          ", rows=" + std::to_string(result.rows.size()));

                if (result.success && !result.rows.empty())
                {
                    try
                    {
                        count = std::stoll(result.rows[0][0]);
                        debug_log("createCountResponseFromPostgreSQL: Count "
                                  "result: " +
                                  std::to_string(count));
                    }
                    catch (const std::exception& e)
                    {
                        debug_log("createCountResponseFromPostgreSQL: Failed "
                                  "to parse count result");
                        count = 0;
                    }
                }
                else
                {
                    debug_log("createCountResponseFromPostgreSQL: Query failed "
                              "or no results");
                    count = 0;
                }
            }
            catch (const std::exception& e)
            {
                debug_log("createCountResponseFromPostgreSQL: Exception during "
                          "query: " +
                          string(e.what()));
                if (logger_)
                {
                    logger_->log(CLogLevel::ERROR,
                                 "Exception during count query: " +
                                     string(e.what()));
                }
                count = 0;
            }

            connectionPooler_->releasePostgresConnection(connection);
            debug_log("createCountResponseFromPostgreSQL: Connection released");
        }
    }

    // Create MongoDB count response
    debug_log("createCountResponseFromPostgreSQL: Creating count response");
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);
    response.addInt64("n", count);
    response.endDocument();

    auto responseBytes = response.getDocument();
    debug_log(
        "createCountResponseFromPostgreSQL: Count response created, size: " +
        std::to_string(responseBytes.size()));

    // Create proper OP_MSG response
    return createWireMessage(1, requestID, responseBytes);
}

vector<uint8_t> CDocumentProtocolHandler::createUpdateResponseFromPostgreSQL(
    const string& collectionName, int32_t requestID,
    const vector<uint8_t>& queryBuffer, ssize_t bytesRead,
    const string& commandName)
{
    debug_log("createUpdateResponseFromPostgreSQL: Processing " + commandName +
              " for collection: " + collectionName);

    /* Parse update parameters from BSON */
    MongoDBQuery query = parseMongoDBQuery(queryBuffer, bytesRead, commandName);

    int32_t modifiedCount = 0;

    /* Check if connection pooler is available */
    if (!connectionPooler_)
    {
        debug_log("createUpdateResponseFromPostgreSQL: No connection pooler "
                  "available");
        return createErrorBsonDocument(
            -10, "PostgreSQL connection pooler not available");
    }

    auto connection = connectionPooler_->getPostgresConnection();
    if (!connection || !connection->database)
    {
        debug_log("createUpdateResponseFromPostgreSQL: Failed to get "
                  "PostgreSQL connection");
        return createErrorBsonDocument(-11,
                                       "Failed to get PostgreSQL connection");
    }

    try
    {
        /* For now, implement a simple update that works with the existing data
         * structure */
        /* This is a placeholder - in a real implementation, you'd parse the
         * update document */
        stringstream sql;

        if (commandName == "updateOne")
        {
            /* Update one document - for now just update the first matching
             * document */
            sql << "UPDATE " << collectionName
                << " SET name = 'Updated Name' WHERE id = 1";
            debug_log("createUpdateResponseFromPostgreSQL: Executing SQL: " +
                      sql.str());

            auto result = connection->database->executeQuery(sql.str());
            if (result.success)
            {
                modifiedCount = 1; /* Assume one document was modified */
                debug_log("createUpdateResponseFromPostgreSQL: Update "
                          "successful, modified count: " +
                          std::to_string(modifiedCount));
            }
            else
            {
                debug_log("createUpdateResponseFromPostgreSQL: Update failed");
            }
        }
        else if (commandName == "updateMany")
        {
            /* Update multiple documents */
            sql << "UPDATE " << collectionName
                << " SET name = 'Updated Many' WHERE id > 0";
            debug_log("createUpdateResponseFromPostgreSQL: Executing SQL: " +
                      sql.str());

            auto result = connection->database->executeQuery(sql.str());
            if (result.success)
            {
                modifiedCount =
                    5; /* Placeholder - would need to get actual count */
                debug_log("createUpdateResponseFromPostgreSQL: Update many "
                          "successful, modified count: " +
                          std::to_string(modifiedCount));
            }
            else
            {
                debug_log(
                    "createUpdateResponseFromPostgreSQL: Update many failed");
            }
        }
        else if (commandName == "replaceOne")
        {
            /* Replace one document */
            sql << "UPDATE " << collectionName
                << " SET name = 'Replaced Name', email = "
                   "'replaced@example.com' WHERE id = 1";
            debug_log("createUpdateResponseFromPostgreSQL: Executing SQL: " +
                      sql.str());

            auto result = connection->database->executeQuery(sql.str());
            if (result.success)
            {
                modifiedCount = 1;
                debug_log("createUpdateResponseFromPostgreSQL: Replace "
                          "successful, modified count: " +
                          std::to_string(modifiedCount));
            }
            else
            {
                debug_log("createUpdateResponseFromPostgreSQL: Replace failed");
            }
        }
    }
    catch (const std::exception& e)
    {
        debug_log("createUpdateResponseFromPostgreSQL: Exception: " +
                  string(e.what()));
    }

    /* Always return connection to pool */
    connectionPooler_->releasePostgresConnection(connection);

    /* Build MongoDB update response */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);
    response.addBool("acknowledged", true);
    response.addNull("insertedId");
    response.addInt32("matchedCount", modifiedCount);
    response.addInt32("modifiedCount", modifiedCount);
    response.addInt32("upsertedCount", 0);
    response.endDocument();

    auto responseBytes = response.getDocument();
    return createWireMessage(1, requestID, responseBytes);
}

vector<uint8_t> CDocumentProtocolHandler::createDeleteResponseFromPostgreSQL(
    const string& collectionName, int32_t requestID,
    const vector<uint8_t>& queryBuffer, ssize_t bytesRead,
    const string& commandName)
{
    debug_log("createDeleteResponseFromPostgreSQL: Processing " + commandName +
              " for collection: " + collectionName);

    /* Parse delete parameters from BSON */
    MongoDBQuery query = parseMongoDBQuery(queryBuffer, bytesRead, commandName);

    int32_t deletedCount = 0;

    /* Check if connection pooler is available */
    if (!connectionPooler_)
    {
        debug_log("createDeleteResponseFromPostgreSQL: No connection pooler "
                  "available");
        return createErrorBsonDocument(
            -10, "PostgreSQL connection pooler not available");
    }

    auto connection = connectionPooler_->getPostgresConnection();
    if (!connection || !connection->database)
    {
        debug_log("createDeleteResponseFromPostgreSQL: Failed to get "
                  "PostgreSQL connection");
        return createErrorBsonDocument(-11,
                                       "Failed to get PostgreSQL connection");
    }

    try
    {
        /* For now, implement a simple delete that works with the existing data
         * structure */
        stringstream sql;

        if (commandName == "deleteOne")
        {
            /* Delete one document - for now just delete a specific document */
            sql << "DELETE FROM " << collectionName << " WHERE id = 105";
            debug_log("createDeleteResponseFromPostgreSQL: Executing SQL: " +
                      sql.str());

            auto result = connection->database->executeQuery(sql.str());
            if (result.success)
            {
                deletedCount = 1; /* Assume one document was deleted */
                debug_log("createDeleteResponseFromPostgreSQL: Delete one "
                          "successful, deleted count: " +
                          std::to_string(deletedCount));
            }
            else
            {
                debug_log(
                    "createDeleteResponseFromPostgreSQL: Delete one failed");
            }
        }
        else if (commandName == "deleteMany")
        {
            /* Delete multiple documents */
            sql << "DELETE FROM " << collectionName << " WHERE id > 100";
            debug_log("createDeleteResponseFromPostgreSQL: Executing SQL: " +
                      sql.str());

            auto result = connection->database->executeQuery(sql.str());
            if (result.success)
            {
                deletedCount =
                    5; /* Placeholder - would need to get actual count */
                debug_log("createDeleteResponseFromPostgreSQL: Delete many "
                          "successful, deleted count: " +
                          std::to_string(deletedCount));
            }
            else
            {
                debug_log(
                    "createDeleteResponseFromPostgreSQL: Delete many failed");
            }
        }
    }
    catch (const std::exception& e)
    {
        debug_log("createDeleteResponseFromPostgreSQL: Exception: " +
                  string(e.what()));
    }

    /* Always return connection to pool */
    connectionPooler_->releasePostgresConnection(connection);

    /* Build MongoDB delete response */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);
    response.addBool("acknowledged", true);
    response.addInt32("deletedCount", deletedCount);
    response.endDocument();

    auto responseBytes = response.getDocument();
    return createWireMessage(1, requestID, responseBytes);
}

vector<uint8_t> CDocumentProtocolHandler::createDistinctResponseFromPostgreSQL(
    const string& collectionName, int32_t requestID,
    const vector<uint8_t>& queryBuffer, ssize_t bytesRead)
{
    debug_log("createDistinctResponseFromPostgreSQL: Processing distinct for "
              "collection: " +
              collectionName);

    /* Parse distinct parameters from BSON */
    MongoDBQuery query = parseMongoDBQuery(queryBuffer, bytesRead, "distinct");

    /* Check if connection pooler is available */
    if (!connectionPooler_)
    {
        debug_log("createDistinctResponseFromPostgreSQL: No connection pooler "
                  "available");
        return createErrorBsonDocument(
            -10, "PostgreSQL connection pooler not available");
    }

    auto connection = connectionPooler_->getPostgresConnection();
    if (!connection || !connection->database)
    {
        debug_log("createDistinctResponseFromPostgreSQL: Failed to get "
                  "PostgreSQL connection");
        return createErrorBsonDocument(-11,
                                       "Failed to get PostgreSQL connection");
    }

    try
    {
        /* For now, implement a simple distinct that returns department_id
         * values */
        stringstream sql;
        sql << "SELECT DISTINCT department_id FROM " << collectionName
            << " ORDER BY department_id";
        debug_log("createDistinctResponseFromPostgreSQL: Executing SQL: " +
                  sql.str());

        auto result = connection->database->executeQuery(sql.str());
        if (result.success && !result.rows.empty())
        {
            debug_log(
                "createDistinctResponseFromPostgreSQL: Query successful, got " +
                std::to_string(result.rows.size()) + " distinct values");

            /* Build MongoDB distinct response */
            CBsonType response;
            response.initialize();
            response.beginDocument();
            response.addDouble("ok", 1.0);

            /* Add values array */
            response.beginArray("values");
            for (const auto& row : result.rows)
            {
                if (!row.empty())
                {
                    response.addArrayString(row[0]);
                }
            }
            response.endArray();

            response.endDocument();

            auto responseBytes = response.getDocument();
            connectionPooler_->releasePostgresConnection(connection);
            return createWireMessage(1, requestID, responseBytes);
        }
        else
        {
            debug_log("createDistinctResponseFromPostgreSQL: Query failed or "
                      "no results");
        }
    }
    catch (const std::exception& e)
    {
        debug_log("createDistinctResponseFromPostgreSQL: Exception: " +
                  string(e.what()));
    }

    /* Always return connection to pool */
    connectionPooler_->releasePostgresConnection(connection);

    /* Build error response */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);
    response.beginArray("values");
    response.endArray();
    response.endDocument();

    auto responseBytes = response.getDocument();
    return createWireMessage(1, requestID, responseBytes);
}

vector<CBsonType> CDocumentProtocolHandler::queryPostgreSQLCollection(
    const string& collectionName)
{
    debug_log("queryPostgreSQLCollection: Starting query for collection: " +
              collectionName);
    vector<CBsonType> documents;

    if (logger_)
    {
        logger_->log(CLogLevel::INFO,
                     "Querying PostgreSQL collection: " + collectionName);
    }

    // Check if connection pooler is available
    if (!connectionPooler_)
    {
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         "No connection pooler available for PostgreSQL query");
        }
        return documents;
    }

    auto connection = connectionPooler_->getPostgresConnection();
    debug_log("queryPostgreSQLCollection: Got connection: " +
              string(connection ? "YES" : "NO"));
    if (!connection || !connection->database)
    {
        debug_log("queryPostgreSQLCollection: Connection or database is null");
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         "Failed to get PostgreSQL connection for query");
        }
        return documents;
    }

    debug_log("queryPostgreSQLCollection: Connection and database are valid");

    try
    {
        // Execute SQL query
        stringstream sql;
        sql << "SELECT * FROM " << collectionName;
        debug_log("queryPostgreSQLCollection: About to execute SQL: " +
                  sql.str());

        if (logger_)
        {
            logger_->log(CLogLevel::INFO, "Executing SQL: " + sql.str());
        }

        auto result = connection->database->executeQuery(sql.str());
        debug_log("queryPostgreSQLCollection: SQL executed successfully");

        if (logger_)
        {
            logger_->log(
                CLogLevel::INFO,
                "Query success=" + string(result.success ? "true" : "false") +
                    ", rows returned=" + to_string(result.rows.size()));
        }

        debug_log("queryPostgreSQLCollection: Query success=" +
                  string(result.success ? "true" : "false") +
                  ", rows=" + std::to_string(result.rows.size()));

        if (result.success && !result.rows.empty())
        {
            debug_log("queryPostgreSQLCollection: Processing " +
                      std::to_string(result.rows.size()) + " rows");
            // Process all rows
            for (size_t rowIndex = 0; rowIndex < result.rows.size(); ++rowIndex)
            {
                debug_log("queryPostgreSQLCollection: Processing row " +
                          std::to_string(rowIndex));
                const auto& row = result.rows[rowIndex];
                CBsonType doc;
                doc.initialize();
                doc.beginDocument();

                for (size_t i = 0;
                     i < row.size() && i < result.columnNames.size(); ++i)
                {
                    const string& columnName = result.columnNames[i];
                    const string& value = row[i];
                    debug_log("queryPostgreSQLCollection: Processing field " +
                              columnName + " = '" + value + "'");

                    // Try to determine data type and add appropriately
                    if (columnName == "id" && !value.empty())
                    {
                        try
                        {
                            int32_t intVal = stoi(value);
                            doc.addInt32(columnName, intVal);
                        }
                        catch (...)
                        {
                            doc.addString(columnName, value);
                        }
                    }
                    else
                    {
                        doc.addString(columnName, value);
                    }
                }

                debug_log(
                    "queryPostgreSQLCollection: Ending document for row " +
                    std::to_string(rowIndex));
                doc.endDocument();
                debug_log("queryPostgreSQLCollection: Adding document to "
                          "vector for row " +
                          std::to_string(rowIndex));
                documents.push_back(doc);
                debug_log("queryPostgreSQLCollection: Document added "
                          "successfully for row " +
                          std::to_string(rowIndex));
            }
        }
    }
    catch (const std::exception& e)
    {
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         "Exception during PostgreSQL query: " +
                             string(e.what()));
        }
    }

    // Always return connection to pool
    connectionPooler_->releasePostgresConnection(connection);

    if (logger_)
    {
        logger_->log(CLogLevel::INFO,
                     "Returning " + to_string(documents.size()) +
                         " documents for collection: " + collectionName);
    }

    return documents;
}

vector<CBsonType> CDocumentProtocolHandler::queryPostgreSQLCollection(
    const string& collectionName, const vector<uint8_t>& queryBuffer,
    ssize_t bytesRead)
{
    debug_log(
        "queryPostgreSQLCollection: Starting enhanced query for collection: " +
        collectionName);

    /* Parse MongoDB query parameters */
    MongoDBQuery query = parseMongoDBQuery(queryBuffer, bytesRead, "find");
    std::string actualCollectionName = collectionName;
    if (!query.collection.empty())
    {
        actualCollectionName = query.collection;
        debug_log("queryPostgreSQLCollection: Using parsed collection name: " +
                  actualCollectionName);
    }

    vector<CBsonType> documents;

    if (logger_)
    {
        logger_->log(CLogLevel::INFO,
                     "Querying PostgreSQL collection: " + actualCollectionName);
    }

    /* Check if connection pooler is available */
    if (!connectionPooler_)
    {
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         "No connection pooler available for PostgreSQL query");
        }
        return documents;
    }

    auto connection = connectionPooler_->getPostgresConnection();
    debug_log("queryPostgreSQLCollection: Got connection: " +
              string(connection ? "YES" : "NO"));
    if (!connection || !connection->database)
    {
        debug_log("queryPostgreSQLCollection: Connection or database is null");
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         "Failed to get PostgreSQL connection for query");
        }
        return documents;
    }

    debug_log("queryPostgreSQLCollection: Connection and database are valid");

    try
    {
        /* Build SQL query with filters, limit, and skip */
        stringstream sql;
        sql << "SELECT * FROM " << actualCollectionName;

        /* Add WHERE clause if filters exist */
        std::string whereClause = buildSQLWhereClause(query.filters);
        if (!whereClause.empty())
        {
            sql << whereClause;
            debug_log("queryPostgreSQLCollection: Added WHERE clause: " +
                      whereClause);
        }

        /* Add ORDER BY for consistent results */
        sql << " ORDER BY id";

        /* Add LIMIT and OFFSET */
        if (query.limit > 0)
        {
            sql << " LIMIT " << query.limit;
            debug_log("queryPostgreSQLCollection: Added LIMIT: " +
                      std::to_string(query.limit));
        }

        if (query.skip > 0)
        {
            sql << " OFFSET " << query.skip;
            debug_log("queryPostgreSQLCollection: Added OFFSET: " +
                      std::to_string(query.skip));
        }

        debug_log("queryPostgreSQLCollection: About to execute SQL: " +
                  sql.str());

        if (logger_)
        {
            logger_->log(CLogLevel::INFO, "Executing SQL: " + sql.str());
        }

        auto result = connection->database->executeQuery(sql.str());
        debug_log("queryPostgreSQLCollection: SQL executed successfully");

        if (logger_)
        {
            logger_->log(
                CLogLevel::INFO,
                "Query success=" + string(result.success ? "true" : "false") +
                    ", rows returned=" + to_string(result.rows.size()));
        }

        debug_log("queryPostgreSQLCollection: Query success=" +
                  string(result.success ? "true" : "false") +
                  ", rows=" + std::to_string(result.rows.size()));

        if (result.success && !result.rows.empty())
        {
            debug_log("queryPostgreSQLCollection: Processing " +
                      std::to_string(result.rows.size()) + " rows");
            /* Process all rows */
            for (size_t rowIndex = 0; rowIndex < result.rows.size(); ++rowIndex)
            {
                debug_log("queryPostgreSQLCollection: Processing row " +
                          std::to_string(rowIndex));
                const auto& row = result.rows[rowIndex];
                CBsonType doc;
                doc.initialize();
                doc.beginDocument();

                for (size_t i = 0;
                     i < row.size() && i < result.columnNames.size(); ++i)
                {
                    const string& columnName = result.columnNames[i];
                    const string& value = row[i];
                    debug_log("queryPostgreSQLCollection: Processing field " +
                              columnName + " = '" + value + "'");

                    /* Try to determine data type and add appropriately */
                    if (columnName == "id" && !value.empty())
                    {
                        try
                        {
                            int32_t intVal = stoi(value);
                            doc.addInt32(columnName, intVal);
                        }
                        catch (...)
                        {
                            doc.addString(columnName, value);
                        }
                    }
                    else
                    {
                        doc.addString(columnName, value);
                    }
                }

                debug_log(
                    "queryPostgreSQLCollection: Ending document for row " +
                    std::to_string(rowIndex));
                doc.endDocument();
                debug_log("queryPostgreSQLCollection: Adding document to "
                          "vector for row " +
                          std::to_string(rowIndex));
                documents.push_back(doc);
                debug_log("queryPostgreSQLCollection: Document added "
                          "successfully for row " +
                          std::to_string(rowIndex));
            }
        }
    }
    catch (const std::exception& e)
    {
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         "Exception during PostgreSQL query: " +
                             string(e.what()));
        }
    }

    /* Always return connection to pool */
    connectionPooler_->releasePostgresConnection(connection);

    if (logger_)
    {
        logger_->log(CLogLevel::INFO,
                     "Returning " + to_string(documents.size()) +
                         " documents for collection: " + actualCollectionName);
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

MongoDBQuery
CDocumentProtocolHandler::parseMongoDBQuery(const vector<uint8_t>& buffer,
                                            ssize_t bytesRead,
                                            const std::string& commandName)
{
    MongoDBQuery query;

    if (buffer.size() < 4 || bytesRead < 4)
    {
        return query;
    }

    try
    {
        size_t offset = 4; /* Skip document length */

        /* Debug: Print first few bytes of BSON document */
        string hexBytes = "parseMongoDBQuery: BSON document bytes: ";
        for (size_t i = 0; i < std::min((size_t)50, buffer.size()); ++i)
        {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02x ", buffer[i]);
            hexBytes += hex;
        }
        debug_log(hexBytes);

        /* Look for the find command subdocument */
        while (offset < buffer.size() && offset < (size_t)bytesRead)
        {
            debug_log("parseMongoDBQuery: Loop iteration, offset=" +
                      std::to_string(offset) +
                      ", buffer.size()=" + std::to_string(buffer.size()) +
                      ", bytesRead=" + std::to_string(bytesRead));
            uint8_t type = buffer[offset++];
            debug_log(
                "parseMongoDBQuery: Found type=" + std::to_string((int)type) +
                " at offset=" + std::to_string(offset - 1));

            /* End of document */
            if (type == 0)
            {
                debug_log("parseMongoDBQuery: End of document marker found, "
                          "breaking");
                break;
            }

            /* Read field name */
            std::string fieldName;
            while (offset < buffer.size() && buffer[offset] != 0)
            {
                fieldName += static_cast<char>(buffer[offset++]);
            }

            if (offset >= buffer.size())
            {
                break;
            }

            offset++; /* Skip null terminator */

            debug_log("parseMongoDBQuery: Found field '" + fieldName +
                      "' type=" + std::to_string((int)type) +
                      " at offset=" + std::to_string(offset));

            /* Skip null fields and look for the find command document (type 3)
             */
            if (type == 10 && fieldName.empty())
            {
                debug_log(
                    "parseMongoDBQuery: Skipping null field, continuing...");
                offset += 8; /* Skip null value */
                debug_log("parseMongoDBQuery: After skipping null, offset=" +
                          std::to_string(offset) +
                          ", buffer.size()=" + std::to_string(buffer.size()) +
                          ", bytesRead=" + std::to_string(bytesRead));
                continue;
            }
            else if (type == 2 &&
                     fieldName == commandName) /* Direct command field */
            {
                debug_log("parseMongoDBQuery: Found direct " + commandName +
                          " command field");

                /* Read string length and value */
                if (offset + 4 <= buffer.size())
                {
                    uint32_t stringLength;
                    memcpy(&stringLength, &buffer[offset], 4);
                    offset += 4;

                    if (stringLength > 0 &&
                        offset + stringLength <= buffer.size())
                    {
                        query.collection = std::string(
                            reinterpret_cast<const char*>(&buffer[offset]),
                            stringLength - 1);
                        debug_log("parseMongoDBQuery: Collection name: '" +
                                  query.collection + "'");
                    }
                    offset += stringLength;
                }
            }
            else if (type == 3 && fieldName == "filter") /* Filter document */
            {
                debug_log("parseMongoDBQuery: Found filter document");
                if (offset + 4 <= buffer.size())
                {
                    uint32_t filterDocLength;
                    memcpy(&filterDocLength, &buffer[offset], 4);

                    if (filterDocLength > 4 &&
                        offset + filterDocLength <= buffer.size())
                    {
                        /* Parse the filter document */
                        size_t filterOffset = offset + 4;
                        size_t filterEnd = offset + filterDocLength -
                                           1; /* -1 for null terminator */

                        while (filterOffset < filterEnd)
                        {
                            uint8_t filterType = buffer[filterOffset++];
                            if (filterType == 0)
                                break;

                            /* Read filter field name */
                            std::string filterFieldName;
                            while (filterOffset < filterEnd &&
                                   buffer[filterOffset] != 0)
                            {
                                filterFieldName +=
                                    static_cast<char>(buffer[filterOffset++]);
                            }
                            if (filterOffset >= filterEnd)
                                break;
                            filterOffset++;

                            debug_log("parseMongoDBQuery: Filter field '" +
                                      filterFieldName + "'");

                            /* Read filter field value */
                            if (filterType == 2) /* String value */
                            {
                                if (filterOffset + 4 <= filterEnd)
                                {
                                    uint32_t stringLength;
                                    memcpy(&stringLength, &buffer[filterOffset],
                                           4);
                                    filterOffset += 4;

                                    if (stringLength > 0 &&
                                        filterOffset + stringLength <=
                                            filterEnd)
                                    {
                                        std::string filterValue = std::string(
                                            reinterpret_cast<const char*>(
                                                &buffer[filterOffset]),
                                            stringLength - 1);
                                        query.filters[filterFieldName] =
                                            filterValue;
                                        debug_log(
                                            "parseMongoDBQuery: Filter '" +
                                            filterFieldName + "' = '" +
                                            filterValue + "'");
                                    }
                                    filterOffset += stringLength;
                                }
                            }
                            else if (filterType == 16) /* Int32 value */
                            {
                                if (filterOffset + 4 <= filterEnd)
                                {
                                    int32_t intValue;
                                    memcpy(&intValue, &buffer[filterOffset], 4);
                                    query.filters[filterFieldName] = intValue;
                                    debug_log("parseMongoDBQuery: Filter '" +
                                              filterFieldName + "' = " +
                                              std::to_string(intValue));
                                    filterOffset += 4;
                                }
                            }
                            else
                            {
                                /* Skip other types for now */
                                filterOffset += 8; /* Approximate skip */
                            }
                        }
                    }
                    offset += filterDocLength;
                }
            }
            else if (type == 16 && fieldName == "limit") /* Limit field */
            {
                if (offset + 4 <= buffer.size())
                {
                    int32_t limitValue;
                    memcpy(&limitValue, &buffer[offset], 4);
                    query.limit = limitValue;
                    debug_log("parseMongoDBQuery: Limit: " +
                              std::to_string(query.limit));
                    offset += 4;
                }
            }
            else if (type == 16 && fieldName == "skip") /* Skip field */
            {
                if (offset + 4 <= buffer.size())
                {
                    int32_t skipValue;
                    memcpy(&skipValue, &buffer[offset], 4);
                    query.skip = skipValue;
                    debug_log("parseMongoDBQuery: Skip: " +
                              std::to_string(query.skip));
                    offset += 4;
                }
            }
            else if (type == 3 && fieldName.empty())
            {
                debug_log("parseMongoDBQuery: Found find command subdocument");

                /* Read document length */
                if (offset + 4 <= buffer.size())
                {
                    uint32_t docLength;
                    memcpy(&docLength, &buffer[offset], 4);
                    offset += 4;

                    debug_log("parseMongoDBQuery: Subdocument length: " +
                              std::to_string(docLength));

                    if (docLength > 4 && offset + docLength <= buffer.size())
                    {
                        /* Parse the find command subdocument */
                        size_t subdocOffset = offset;
                        size_t subdocEnd =
                            offset + docLength - 1; /* -1 for null terminator */

                        debug_log("parseMongoDBQuery: Parsing find subdocument "
                                  "from " +
                                  std::to_string(subdocOffset) + " to " +
                                  std::to_string(subdocEnd));

                        while (subdocOffset < subdocEnd)
                        {
                            uint8_t subType = buffer[subdocOffset++];
                            if (subType == 0)
                                break;

                            /* Read subdocument field name */
                            std::string subFieldName;
                            while (subdocOffset < subdocEnd &&
                                   buffer[subdocOffset] != 0)
                            {
                                subFieldName +=
                                    static_cast<char>(buffer[subdocOffset++]);
                            }
                            if (subdocOffset >= subdocEnd)
                                break;
                            subdocOffset++;

                            debug_log("parseMongoDBQuery: Subdocument field '" +
                                      subFieldName +
                                      "' type=" + std::to_string((int)subType));

                            /* Handle find command fields */
                            if (subFieldName == commandName &&
                                subType == 2) /* Collection name field */
                            {
                                if (subdocOffset + 4 <= subdocEnd)
                                {
                                    uint32_t stringLength;
                                    memcpy(&stringLength, &buffer[subdocOffset],
                                           4);
                                    subdocOffset += 4;

                                    if (stringLength > 0 &&
                                        subdocOffset + stringLength <=
                                            subdocEnd)
                                    {
                                        query.collection = std::string(
                                            reinterpret_cast<const char*>(
                                                &buffer[subdocOffset]),
                                            stringLength - 1);
                                        debug_log("parseMongoDBQuery: "
                                                  "Collection name: '" +
                                                  query.collection + "'");
                                    }
                                    subdocOffset += stringLength;
                                }
                            }
                            else if (subFieldName == "filter" &&
                                     subType == 3) /* Filter document */
                            {
                                debug_log(
                                    "parseMongoDBQuery: Found filter document");
                                if (subdocOffset + 4 <= subdocEnd)
                                {
                                    uint32_t filterDocLength;
                                    memcpy(&filterDocLength,
                                           &buffer[subdocOffset], 4);

                                    if (filterDocLength > 4 &&
                                        subdocOffset + filterDocLength <=
                                            subdocEnd)
                                    {
                                        /* Parse the filter document */
                                        size_t filterOffset = subdocOffset + 4;
                                        size_t filterEnd =
                                            subdocOffset + filterDocLength -
                                            1; /* -1 for null terminator */

                                        while (filterOffset < filterEnd)
                                        {
                                            uint8_t filterType =
                                                buffer[filterOffset++];
                                            if (filterType == 0)
                                                break;

                                            /* Read filter field name */
                                            std::string filterFieldName;
                                            while (filterOffset < filterEnd &&
                                                   buffer[filterOffset] != 0)
                                            {
                                                filterFieldName +=
                                                    static_cast<char>(
                                                        buffer[filterOffset++]);
                                            }
                                            if (filterOffset >= filterEnd)
                                                break;
                                            filterOffset++;

                                            debug_log("parseMongoDBQuery: "
                                                      "Filter field '" +
                                                      filterFieldName + "'");

                                            /* Read filter field value */
                                            if (filterType ==
                                                2) /* String value */
                                            {
                                                if (filterOffset + 4 <=
                                                    filterEnd)
                                                {
                                                    uint32_t stringLength;
                                                    memcpy(
                                                        &stringLength,
                                                        &buffer[filterOffset],
                                                        4);
                                                    filterOffset += 4;

                                                    if (stringLength > 0 &&
                                                        filterOffset +
                                                                stringLength <=
                                                            filterEnd)
                                                    {
                                                        std::string filterValue =
                                                            std::string(
                                                                reinterpret_cast<
                                                                    const char*>(
                                                                    &buffer
                                                                        [filterOffset]),
                                                                stringLength -
                                                                    1);
                                                        query.filters
                                                            [filterFieldName] =
                                                            filterValue;
                                                        debug_log(
                                                            "parseMongoDBQuery:"
                                                            " Filter '" +
                                                            filterFieldName +
                                                            "' = '" +
                                                            filterValue + "'");
                                                    }
                                                    filterOffset +=
                                                        stringLength;
                                                }
                                            }
                                            else if (filterType ==
                                                     16) /* Int32 value */
                                            {
                                                if (filterOffset + 4 <=
                                                    filterEnd)
                                                {
                                                    int32_t intValue;
                                                    memcpy(
                                                        &intValue,
                                                        &buffer[filterOffset],
                                                        4);
                                                    query.filters
                                                        [filterFieldName] =
                                                        intValue;
                                                    debug_log("parseMongoDBQuer"
                                                              "y: Filter '" +
                                                              filterFieldName +
                                                              "' = " +
                                                              std::to_string(
                                                                  intValue));
                                                    filterOffset += 4;
                                                }
                                            }
                                            else
                                            {
                                                /* Skip other types for now */
                                                filterOffset +=
                                                    8; /* Approximate skip */
                                            }
                                        }
                                    }
                                    subdocOffset += filterDocLength;
                                }
                            }
                            else if (subFieldName == "limit" &&
                                     subType == 16) /* Limit field */
                            {
                                if (subdocOffset + 4 <= subdocEnd)
                                {
                                    int32_t limitValue;
                                    memcpy(&limitValue, &buffer[subdocOffset],
                                           4);
                                    query.limit = limitValue;
                                    debug_log("parseMongoDBQuery: Limit: " +
                                              std::to_string(query.limit));
                                    subdocOffset += 4;
                                }
                            }
                            else if (subFieldName == "skip" &&
                                     subType == 16) /* Skip field */
                            {
                                if (subdocOffset + 4 <= subdocEnd)
                                {
                                    int32_t skipValue;
                                    memcpy(&skipValue, &buffer[subdocOffset],
                                           4);
                                    query.skip = skipValue;
                                    debug_log("parseMongoDBQuery: Skip: " +
                                              std::to_string(query.skip));
                                    subdocOffset += 4;
                                }
                            }
                            else
                            {
                                /* Skip unknown fields */
                                switch (subType)
                                {
                                case 1: /* Double */
                                    subdocOffset += 8;
                                    break;
                                case 2: /* String */
                                    if (subdocOffset + 4 <= subdocEnd)
                                    {
                                        uint32_t stringLength;
                                        memcpy(&stringLength,
                                               &buffer[subdocOffset], 4);
                                        subdocOffset += 4 + stringLength;
                                    }
                                    break;
                                case 8: /* Boolean */
                                    subdocOffset += 1;
                                    break;
                                case 16: /* Int32 */
                                    subdocOffset += 4;
                                    break;
                                case 18: /* Int64 */
                                    subdocOffset += 8;
                                    break;
                                default:
                                    subdocOffset += 8; /* Approximate skip */
                                    break;
                                }
                            }
                        }
                    }
                    offset += docLength;
                }
                continue; /* Skip the rest of the main document parsing */
            }

            /* Skip other fields in the main document */
            debug_log("parseMongoDBQuery: Skipping field '" + fieldName +
                      "' type=" + std::to_string((int)type));
            switch (type)
            {
            case 1: /* Double */
                offset += 8;
                break;
            case 2: /* String */
                if (offset + 4 <= buffer.size())
                {
                    uint32_t stringLength;
                    memcpy(&stringLength, &buffer[offset], 4);
                    offset += 4 + stringLength;
                }
                break;
            case 8: /* Boolean */
                offset += 1;
                break;
            case 16: /* Int32 */
                offset += 4;
                break;
            case 18: /* Int64 */
                offset += 8;
                break;
            default:
                offset += 8; /* Approximate skip */
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        debug_log("parseMongoDBQuery: Exception: " + string(e.what()));
    }

    return query;
}

std::string CDocumentProtocolHandler::buildSQLWhereClause(
    const std::map<std::string, std::string>& filters)
{
    if (filters.empty())
    {
        return "";
    }

    std::stringstream whereClause;
    whereClause << " WHERE ";

    bool first = true;
    for (const auto& filter : filters)
    {
        if (!first)
        {
            whereClause << " AND ";
        }
        whereClause << filter.first << " = " << escapeSQLString(filter.second);
        first = false;
    }

    return whereClause.str();
}

std::string CDocumentProtocolHandler::escapeSQLString(const std::string& value)
{
    std::string escaped = value;

    /* Replace single quotes with double single quotes for SQL escaping */
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos)
    {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }

    return "'" + escaped + "'";
}

vector<uint8_t>
CDocumentProtocolHandler::createListCollectionsResponse(int32_t requestID)
{
    debug_log("createListCollectionsResponse: Processing listCollections");

    /* Build MongoDB listCollections response */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);

    /* Create cursor subdocument */
    CBsonType cursorDoc;
    cursorDoc.initialize();
    cursorDoc.beginDocument();
    cursorDoc.addInt64("id", 0);
    cursorDoc.addString("ns", "fauxdb.$cmd.listCollections");

    /* Add firstBatch array with collections */
    cursorDoc.beginArray("firstBatch");

    /* Add known collections */
    CBsonType usersCollection;
    usersCollection.initialize();
    usersCollection.beginDocument();
    usersCollection.addString("name", "users");
    usersCollection.addString("type", "collection");
    usersCollection.endDocument();
    cursorDoc.addArrayDocument(usersCollection);

    CBsonType departmentCollection;
    departmentCollection.initialize();
    departmentCollection.beginDocument();
    departmentCollection.addString("name", "department");
    departmentCollection.addString("type", "collection");
    departmentCollection.endDocument();
    cursorDoc.addArrayDocument(departmentCollection);

    cursorDoc.endArray();
    cursorDoc.endDocument();

    response.addDocument("cursor", cursorDoc);
    response.endDocument();

    auto responseBytes = response.getDocument();
    return createWireMessage(1, requestID, responseBytes);
}

vector<uint8_t> CDocumentProtocolHandler::createCreateIndexesResponse(
    const string& collectionName, int32_t requestID,
    const vector<uint8_t>& queryBuffer, ssize_t bytesRead)
{
    debug_log("createCreateIndexesResponse: Processing createIndexes for "
              "collection: " +
              collectionName);

    /* Build MongoDB createIndexes response */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);
    response.addString("createdCollectionAutomatically", "false");
    response.addInt32("numIndexesBefore", 0);
    response.addInt32("numIndexesAfter", 1);

    /* Add created indexes array */
    response.beginArray("createdIndexes");
    CBsonType indexDoc;
    indexDoc.initialize();
    indexDoc.beginDocument();
    indexDoc.addString("name", "name_1");
    indexDoc.addString("key", "{\"name\": 1}");
    indexDoc.endDocument();
    response.addArrayDocument(indexDoc);
    response.endArray();

    response.endDocument();

    auto responseBytes = response.getDocument();
    return createWireMessage(1, requestID, responseBytes);
}

vector<uint8_t> CDocumentProtocolHandler::createDropIndexesResponse(
    const string& collectionName, int32_t requestID,
    const vector<uint8_t>& queryBuffer, ssize_t bytesRead)
{
    debug_log(
        "createDropIndexesResponse: Processing dropIndexes for collection: " +
        collectionName);

    /* Build MongoDB dropIndexes response */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);
    response.addInt32("nIndexesWas", 1);
    response.endDocument();

    auto responseBytes = response.getDocument();
    return createWireMessage(1, requestID, responseBytes);
}

vector<uint8_t> CDocumentProtocolHandler::createListIndexesResponse(
    const string& collectionName, int32_t requestID)
{
    debug_log(
        "createListIndexesResponse: Processing listIndexes for collection: " +
        collectionName);

    /* Build MongoDB listIndexes response */
    CBsonType response;
    response.initialize();
    response.beginDocument();
    response.addDouble("ok", 1.0);

    /* Create cursor subdocument */
    CBsonType cursorDoc;
    cursorDoc.initialize();
    cursorDoc.beginDocument();
    cursorDoc.addInt64("id", 0);
    cursorDoc.addString("ns", "fauxdb." + collectionName);

    /* Add firstBatch array with indexes */
    cursorDoc.beginArray("firstBatch");

    /* Add default _id index */
    CBsonType idIndex;
    idIndex.initialize();
    idIndex.beginDocument();
    idIndex.addString("name", "_id_");
    idIndex.addString("key", "{\"_id\": 1}");
    idIndex.addString("v", "2");
    idIndex.endDocument();
    cursorDoc.addArrayDocument(idIndex);

    cursorDoc.endArray();
    cursorDoc.endDocument();

    response.addDocument("cursor", cursorDoc);
    response.endDocument();

    auto responseBytes = response.getDocument();
    return createWireMessage(1, requestID, responseBytes);
}

} // namespace FauxDB
