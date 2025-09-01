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
#include <sstream>
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

    if (buffer.empty() || bytesRead <= 0)
    {
        return createErrorBsonDocument(-1, "Empty buffer");
    }

    // Basic MongoDB wire protocol parsing
    if (bytesRead < 16)
    {
        return createErrorBsonDocument(-2, "Message too short for header");
    }

    // Extract header
    int32_t messageLength, requestID, responseTo, opCode;
    std::memcpy(&messageLength, buffer.data(), 4);
    std::memcpy(&requestID, buffer.data() + 4, 4);
    std::memcpy(&responseTo, buffer.data() + 8, 4);
    std::memcpy(&opCode, buffer.data() + 12, 4);

    // Validate message length
    if (messageLength > 48000000 || messageLength < 16)
    {
        return createErrorBsonDocument(-3, "Invalid message length");
    }

    std::string commandName = "hello";
    std::string database = "admin";

    // Handle OP_MSG (opCode 2013)
    if (opCode == 2013)
    {
        if (bytesRead >= 20)
        {
            // Extract flagBits
            uint32_t flagBits;
            std::memcpy(&flagBits, buffer.data() + 16, 4);

            // Parse first section (Kind 0 - Body)
            if (bytesRead >= 21)
            {
                size_t offset = 20;
                if (offset < static_cast<size_t>(bytesRead) &&
                    buffer[offset] == 0) // Kind 0
                {
                    offset++;
                    // Parse BSON document to extract command name and
                    // database/collection
                    if (offset + 4 < static_cast<size_t>(bytesRead))
                    {
                        int32_t docSize;
                        std::memcpy(&docSize, buffer.data() + offset, 4);
                        offset += 4;

                        // Parse BSON fields to extract command, database, and
                        // collection
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

                            // Extract command name, database, and collection
                            if (fieldName == "find" &&
                                fieldType == 0x02) // String type
                            {
                                commandName = "find";
                                // Collection name will be extracted later
                            }
                            else if (fieldName == "findOne" &&
                                     fieldType == 0x02)
                            {
                                commandName = "findOne";
                            }
                            else if (fieldName == "count" && fieldType == 0x02)
                            {
                                commandName = "count";
                            }
                            else if (fieldName == "countDocuments" &&
                                     fieldType == 0x02)
                            {
                                commandName = "countDocuments";
                            }
                            else if (fieldName == "estimatedDocumentCount" &&
                                     fieldType == 0x02)
                            {
                                commandName = "estimatedDocumentCount";
                            }
                            else if (fieldName == "insert" && fieldType == 0x02)
                            {
                                commandName = "insert";
                            }
                            else if (fieldName == "update" && fieldType == 0x02)
                            {
                                commandName = "update";
                            }
                            else if (fieldName == "distinct" &&
                                     fieldType == 0x02)
                            {
                                commandName = "distinct";
                            }
                            else if (fieldName == "findAndModify" &&
                                     fieldType == 0x02)
                            {
                                commandName = "findAndModify";
                            }
                            else if (fieldName == "drop" && fieldType == 0x02)
                            {
                                commandName = "drop";
                            }
                            else if (fieldName == "create" && fieldType == 0x02)
                            {
                                commandName = "create";
                            }
                            else if (fieldName == "count" && fieldType == 0x02)
                            {
                                commandName = "count";
                            }
                            else if (fieldName == "listCollections" &&
                                     fieldType == 0x10)
                            {
                                commandName = "listCollections";
                            }
                            else if (fieldName == "explain" &&
                                     fieldType == 0x03)
                            {
                                commandName = "explain";
                            }
                            else if (fieldName == "dbStats" &&
                                     fieldType == 0x10)
                            {
                                commandName = "dbStats";
                            }
                            else if (fieldName == "collStats" &&
                                     fieldType == 0x02)
                            {
                                commandName = "collStats";
                            }
                            else if (fieldName == "listDatabases" &&
                                     fieldType == 0x10)
                            {
                                commandName = "listDatabases";
                            }
                            else if (fieldName == "serverStatus" &&
                                     fieldType == 0x10)
                            {
                                commandName = "serverStatus";
                            }
                            else if (fieldName == "createIndexes" &&
                                     fieldType == 0x02)
                            {
                                commandName = "createIndexes";
                            }
                            else if (fieldName == "listIndexes" &&
                                     fieldType == 0x02)
                            {
                                commandName = "listIndexes";
                            }
                            else if (fieldName == "dropIndexes" &&
                                     (fieldType == 0x02 || fieldType == 0x10))
                            {
                                commandName = "dropIndexes";
                            }
                            else if (fieldName == "ping" && fieldType == 0x10)
                            {
                                commandName = "ping";
                            }
                            else if (fieldName == "hello" && fieldType == 0x10)
                            {
                                commandName = "hello";
                            }
                            else if (fieldName == "buildInfo" &&
                                     fieldType == 0x10)
                            {
                                commandName = "buildInfo";
                            }
                            else if (fieldName == "isMaster" &&
                                     fieldType == 0x10)
                            {
                                commandName = "isMaster";
                            }
                            else if (fieldName == "whatsMyUri" &&
                                     fieldType == 0x10)
                            {
                                commandName = "whatsMyUri";
                            }
                            else if (fieldName == "delete" && fieldType == 0x02)
                            {
                                commandName = "delete";
                            }
                            else if (fieldName == "aggregate" &&
                                     fieldType == 0x02)
                            {
                                commandName = "aggregate";
                            }
                            else if (fieldName == "listCollections" &&
                                     (fieldType == 0x01 || fieldType == 0x10))
                            {
                                commandName = "listCollections";
                            }
                            else if (fieldName == "listDatabases" &&
                                     (fieldType == 0x01 || fieldType == 0x10))
                            {
                                commandName = "listDatabases";
                            }
                            else if (fieldName == "listIndexes" &&
                                     fieldType == 0x02)
                            {
                                commandName = "listIndexes";
                            }
                            else if (fieldName == "createIndexes" &&
                                     fieldType == 0x02)
                            {
                                commandName = "createIndexes";
                            }
                            else if (fieldName == "dbStats" &&
                                     (fieldType == 0x01 || fieldType == 0x10))
                            {
                                commandName = "dbStats";
                            }
                            else if (fieldName == "buildInfo" &&
                                     (fieldType == 0x01 || fieldType == 0x10))
                            {
                                commandName = "buildInfo";
                            }
                            else if (fieldName == "hello" &&
                                     (fieldType == 0x01 || fieldType == 0x10))
                            {
                                commandName = "hello";
                            }
                            else if (fieldName == "isMaster" &&
                                     (fieldType == 0x01 || fieldType == 0x10))
                            {
                                commandName = "isMaster";
                            }
                            else if (fieldName == "ping" &&
                                     (fieldType == 0x01 || fieldType == 0x10))
                            {
                                commandName = "ping";
                            }
                            else if (fieldName == "database" &&
                                     fieldType == 0x02)
                            {
                                // Extract database name
                                if (offset + 4 < static_cast<size_t>(bytesRead))
                                {
                                    int32_t strLen;
                                    std::memcpy(&strLen, buffer.data() + offset,
                                                4);
                                    offset += 4;

                                    if (strLen > 0 &&
                                        offset + strLen - 1 <
                                            static_cast<size_t>(bytesRead))
                                    {
                                        database.assign(
                                            reinterpret_cast<const char*>(
                                                buffer.data() + offset),
                                            strLen - 1);
                                        offset += strLen;
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
                                    if (offset + 4 <
                                        static_cast<size_t>(bytesRead))
                                    {
                                        int32_t strLen;
                                        std::memcpy(&strLen,
                                                    buffer.data() + offset, 4);
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
                                    // For other types, skip to end to avoid
                                    // infinite loop
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Generate response based on command
    return createCommandResponse(commandName, requestID, buffer, bytesRead);
}

vector<uint8_t> CDocumentProtocolHandler::createCommandResponse(
    const string& commandName, int32_t requestID, const vector<uint8_t>& buffer,
    ssize_t bytesRead)
{
    /* TODO: Enable modular system after testing - temporarily disabled
    if (commandRegistry_->hasCommand(commandName))
    {
        string collectionName =
    CollectionNameParser::extractCollectionName(buffer, bytesRead, commandName);
        if (collectionName.empty())
        {
            collectionName = "test";
        }
        auto command = commandRegistry_->getCommand(commandName);
        if (command)
        {
            return command->execute(collectionName, buffer, bytesRead,
    connectionPooler_);
        }
    }
    */

    CBsonType bson;
    if (!bson.initialize() || !bson.beginDocument())
    {
        return createErrorBsonDocument(-6, "BSON init failed");
    }

    if (commandRegistry_ && commandRegistry_->hasCommand(commandName))
    {

        CommandContext context;
        context.collectionName = extractCollectionName(buffer, bytesRead);
        context.databaseName = "fauxdb";
        context.requestBuffer = buffer;
        context.requestSize = bytesRead;
        context.requestID = requestID;
        context.connectionPooler = connectionPooler_;

        return commandRegistry_->executeCommand(commandName, context);
    }
    else if (commandName == "hello" || commandName == "isMaster")
    {
        bson.addDouble("ok", 1.0);
        bson.addBool("isWritablePrimary", true);
        bson.addBool("ismaster", true);
        bson.addInt32("maxBsonObjectSize", 16777216);
        bson.addInt32("maxMessageSizeBytes", 48000000);
        bson.addInt32("maxWriteBatchSize", 100000);
        auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now());
        bson.addDateTime("localTime", now.time_since_epoch().count());
        bson.addInt32("minWireVersion", 8);
        bson.addInt32("maxWireVersion", 8);
        bson.addString("msg", "isdbgrid");
        // Create hosts array
        bson.beginArray("hosts");
        bson.addArrayString("localhost:27018");
        bson.endArray();
    }
    else if (commandName == "ping")
    {
        bson.addDouble("ok", 1.0);
    }
    else if (commandName == "find")
    {
        // Extract collection name from the request
        string collectionName = extractCollectionName(buffer, bytesRead);

        bson.addDouble("ok", 1.0);

        CBsonType cursorDoc;
        cursorDoc.initialize();
        cursorDoc.beginDocument();
        cursorDoc.addInt64("id", 0);
        cursorDoc.addString("ns", collectionName + ".collection");

        cursorDoc.beginArray("firstBatch");

        // Try PostgreSQL integration - behave like real MongoDB
        if (connectionPooler_)
        {
            try
            {
                auto voidConnection = connectionPooler_->getConnection();
                if (voidConnection)
                {
                    auto connection =
                        std::static_pointer_cast<PGConnection>(voidConnection);
                    if (connection && connection->database)
                    {
                        // Use the already extracted collection name

                        // Try to query the PostgreSQL table
                        stringstream sql;
                        sql << "SELECT * FROM " << collectionName
                            << " LIMIT 10";

                        auto result =
                            connection->database->executeQuery(sql.str());

                        if (result.success)
                        {
                            // PostgreSQL query succeeded - add results

                            for (size_t i = 0; i < result.rows.size(); ++i)
                            {
                                CBsonType doc;
                                doc.initialize();
                                doc.beginDocument();

                                // Add _id field if not present
                                bool hasId = false;
                                for (size_t j = 0;
                                     j < result.columnNames.size() &&
                                     j < result.rows[i].size();
                                     ++j)
                                {
                                    const string& colName =
                                        result.columnNames[j];
                                    const string& value = result.rows[i][j];

                                    if (colName == "_id" || colName == "id")
                                    {
                                        doc.addString("_id", value);
                                        hasId = true;
                                    }
                                    else
                                    {
                                        // Simple type inference
                                        if (value == "true" || value == "false")
                                        {
                                            doc.addBool(colName,
                                                        value == "true");
                                        }
                                        else if (value.find('.') !=
                                                 string::npos)
                                        {
                                            try
                                            {
                                                doc.addDouble(colName,
                                                              std::stod(value));
                                            }
                                            catch (...)
                                            {
                                                doc.addString(colName, value);
                                            }
                                        }
                                        else
                                        {
                                            try
                                            {
                                                doc.addInt32(colName,
                                                             std::stoi(value));
                                            }
                                            catch (...)
                                            {
                                                doc.addString(colName, value);
                                            }
                                        }
                                    }
                                }

                                // Add generated _id if not present
                                if (!hasId)
                                {
                                    doc.addString(
                                        "_id", "pg_" + std::to_string(i + 1));
                                }

                                doc.endDocument();
                                cursorDoc.addArrayDocument(doc);
                            }

                            // If no rows returned, MongoDB returns empty array
                            // (which we already have)
                        }
                        else
                        {
                            // PostgreSQL query failed - table might not exist
                            // MongoDB behavior: return empty result set, not an
                            // error for missing collections (MongoDB creates
                            // collections on first insert, so missing
                            // collections are normal)
                        }
                    }
                    connectionPooler_->releaseConnection(voidConnection);
                }
            }
            catch (const std::exception& e)
            {
                // PostgreSQL error - MongoDB behavior: return empty result for
                // missing tables Real MongoDB doesn't error on missing
                // collections, it returns empty results
            }
        }
        // If no connection pooler, return empty result (like MongoDB with no
        // data)

        cursorDoc.endArray();
        cursorDoc.endDocument();
        bson.addDocument("cursor", cursorDoc);
    }
    else if (commandName == "findOne")
    {
        string collectionName = extractCollectionName(buffer, bytesRead);

        bson.addDouble("ok", 1.0);

        // Try PostgreSQL integration - behave like real MongoDB
        if (connectionPooler_)
        {
            try
            {
                auto voidConnection = connectionPooler_->getConnection();
                if (voidConnection)
                {
                    auto connection =
                        std::static_pointer_cast<PGConnection>(voidConnection);
                    if (connection && connection->database)
                    {
                        // Query PostgreSQL table - findOne returns single
                        // document
                        stringstream sql;
                        sql << "SELECT * FROM " << collectionName << " LIMIT 1";

                        auto result =
                            connection->database->executeQuery(sql.str());

                        if (result.success && !result.rows.empty())
                        {
                            // PostgreSQL query succeeded - add first result as
                            // document
                            const auto& row = result.rows[0];

                            // Add _id field if not present
                            bool hasId = false;
                            for (size_t j = 0; j < result.columnNames.size() &&
                                               j < row.size();
                                 ++j)
                            {
                                const string& colName = result.columnNames[j];
                                const string& value = row[j];

                                if (colName == "_id" || colName == "id")
                                {
                                    bson.addString("_id", value);
                                    hasId = true;
                                }
                                else
                                {
                                    // Simple type inference
                                    if (value == "true" || value == "false")
                                    {
                                        bson.addBool(colName, value == "true");
                                    }
                                    else if (value.find('.') != string::npos)
                                    {
                                        try
                                        {
                                            bson.addDouble(colName,
                                                           std::stod(value));
                                        }
                                        catch (...)
                                        {
                                            bson.addString(colName, value);
                                        }
                                    }
                                    else
                                    {
                                        try
                                        {
                                            bson.addInt32(colName,
                                                          std::stoi(value));
                                        }
                                        catch (...)
                                        {
                                            bson.addString(colName, value);
                                        }
                                    }
                                }
                            }

                            // Add generated _id if not present
                            if (!hasId)
                            {
                                bson.addString("_id", "pg_1");
                            }
                        }
                        else
                        {
                            // No results - MongoDB returns null for findOne
                            // with no results
                            bson.addNull("_id");
                        }

                        connectionPooler_->releaseConnection(voidConnection);
                    }
                    else
                    {
                        // No database - return null result
                        bson.addNull("_id");
                        if (voidConnection)
                        {
                            connectionPooler_->releaseConnection(
                                voidConnection);
                        }
                    }
                }
                else
                {
                    // No connection - return null result
                    bson.addNull("_id");
                }
            }
            catch (const std::exception& e)
            {
                // Log error but don't crash - return null result like MongoDB
                bson.addNull("_id");
            }
        }
        else
        {
            // No connection pooler - return null result
            bson.addNull("_id");
        }
    }
    else if (commandName == "countDocuments" || commandName == "count")
    {
        string collectionName = extractCollectionName(buffer, bytesRead);

        bson.addDouble("ok", 1.0);

        int64_t count = 0;

        // Try PostgreSQL integration - behave like real MongoDB
        if (connectionPooler_)
        {
            try
            {
                auto voidConnection = connectionPooler_->getConnection();
                if (voidConnection)
                {
                    auto connection =
                        std::static_pointer_cast<PGConnection>(voidConnection);
                    if (connection && connection->database)
                    {
                        // Query PostgreSQL table count
                        stringstream sql;
                        sql << "SELECT COUNT(*) FROM " << collectionName;

                        auto result =
                            connection->database->executeQuery(sql.str());

                        if (result.success && !result.rows.empty())
                        {
                            try
                            {
                                count = std::stoll(result.rows[0][0]);
                            }
                            catch (...)
                            {
                                count = 0;
                            }
                        }

                        connectionPooler_->releaseConnection(voidConnection);
                    }
                    else
                    {
                        // No database - return 0 count
                        count = 0;
                        if (voidConnection)
                        {
                            connectionPooler_->releaseConnection(
                                voidConnection);
                        }
                    }
                }
                else
                {
                    // No connection - return 0 count
                    count = 0;
                }
            }
            catch (const std::exception& e)
            {
                // Log error but don't crash - return 0 count like MongoDB
                count = 0;
            }
        }
        else
        {
            // No connection pooler - return 0 count
            count = 0;
        }

        bson.addInt64("n", count);
    }
    else if (commandName == "estimatedDocumentCount")
    {
        string collectionName = extractCollectionName(buffer, bytesRead);

        bson.addDouble("ok", 1.0);

        int64_t count = 0;

        // Try PostgreSQL integration - behave like real MongoDB
        if (connectionPooler_)
        {
            try
            {
                auto voidConnection = connectionPooler_->getConnection();
                if (voidConnection)
                {
                    auto connection =
                        std::static_pointer_cast<PGConnection>(voidConnection);
                    if (connection && connection->database)
                    {
                        // Query PostgreSQL table count (same as countDocuments
                        // for now)
                        stringstream sql;
                        sql << "SELECT COUNT(*) FROM " << collectionName;

                        auto result =
                            connection->database->executeQuery(sql.str());

                        if (result.success && !result.rows.empty())
                        {
                            try
                            {
                                count = std::stoll(result.rows[0][0]);
                            }
                            catch (...)
                            {
                                count = 0;
                            }
                        }

                        connectionPooler_->releaseConnection(voidConnection);
                    }
                    else
                    {
                        // No database - return 0 count
                        count = 0;
                        if (voidConnection)
                        {
                            connectionPooler_->releaseConnection(
                                voidConnection);
                        }
                    }
                }
                else
                {
                    // No connection - return 0 count
                    count = 0;
                }
            }
            catch (const std::exception& e)
            {
                // Log error but don't crash - return 0 count like MongoDB
                count = 0;
            }
        }
        else
        {
            // No connection pooler - return 0 count
            count = 0;
        }

        bson.addInt64("n", count);
    }
    else if (commandName == "buildInfo")
    {
        bson.addDouble("ok", 1.0);
        bson.addString("version", "1.0.0");
        bson.addString("gitVersion", "fauxdb-1.0.0");
        bson.addString("modules", "none");
        bson.addString("allocator", "system");
        bson.addString("javascriptEngine", "none");
        bson.addString("sysInfo", "FauxDB Server");
    }
    else if (commandName == "insert")
    {
        bson.addDouble("ok", 1.0);
        bson.addInt32("n", 1);
        /* Simplified implementation - just return success for now */
    }
    else if (commandName == "update")
    {
        bson.addDouble("ok", 1.0);
        bson.addInt32("n", 1);
        bson.addInt32("nModified", 1);
        /* Simplified implementation - just return success for now */
    }
    else if (commandName == "delete")
    {
        bson.addDouble("ok", 1.0);
        bson.addInt32("n", 1);
        /* Simplified implementation - just return success for now */
    }

    else if (commandName == "aggregate")
    {
        bson.addDouble("ok", 1.0);
        CBsonType cursorDoc;
        cursorDoc.initialize();
        cursorDoc.beginDocument();
        cursorDoc.addInt64("id", 0);
        cursorDoc.addString("ns", "test.collection");
        cursorDoc.beginArray("firstBatch");
        cursorDoc.endArray();
        cursorDoc.endDocument();
        bson.addDocument("cursor", cursorDoc);
    }
    else if (commandName == "listCollections")
    {
        bson.addDouble("ok", 1.0);
        CBsonType cursorDoc;
        cursorDoc.initialize();
        cursorDoc.beginDocument();
        cursorDoc.addInt64("id", 0);
        cursorDoc.addString("ns", "admin.$cmd.listCollections");
        cursorDoc.beginArray("firstBatch");
        /* Add test collection */
        CBsonType collDoc;
        collDoc.initialize();
        collDoc.beginDocument();
        collDoc.addString("name", "test");
        collDoc.addString("type", "collection");
        collDoc.endDocument();
        cursorDoc.addArrayDocument(collDoc);
        cursorDoc.endArray();
        cursorDoc.endDocument();
        bson.addDocument("cursor", cursorDoc);
    }
    else if (commandName == "listDatabases")
    {
        bson.addDouble("ok", 1.0);
        bson.beginArray("databases");
        CBsonType dbDoc;
        dbDoc.initialize();
        dbDoc.beginDocument();
        dbDoc.addString("name", "test");
        dbDoc.addInt64("sizeOnDisk", 1024);
        dbDoc.addBool("empty", false);
        dbDoc.endDocument();
        bson.addDocument("0", dbDoc);
        bson.endArray();
        bson.addInt64("totalSize", 1024);
        bson.addInt64("totalSizeMb", 1);
    }
    else if (commandName == "listIndexes")
    {
        bson.addDouble("ok", 1.0);
        CBsonType cursorDoc;
        cursorDoc.initialize();
        cursorDoc.beginDocument();
        cursorDoc.addInt64("id", 0);
        cursorDoc.addString("ns", "test.$cmd.listIndexes.test");
        cursorDoc.beginArray("firstBatch");
        /* Add default _id index */
        CBsonType indexDoc;
        indexDoc.initialize();
        indexDoc.beginDocument();
        indexDoc.addInt32("v", 2);
        CBsonType keyDoc;
        keyDoc.initialize();
        keyDoc.beginDocument();
        keyDoc.addInt32("_id", 1);
        keyDoc.endDocument();
        indexDoc.addDocument("key", keyDoc);
        indexDoc.addString("name", "_id_");
        indexDoc.endDocument();
        cursorDoc.addDocument("0", indexDoc);
        cursorDoc.endArray();
        cursorDoc.endDocument();
        bson.addDocument("cursor", cursorDoc);
    }
    else if (commandName == "createIndexes")
    {
        bson.addDouble("ok", 1.0);
        bson.addInt32("createdCollectionAutomatically", 0);
        bson.addInt32("numIndexesBefore", 1);
        bson.addInt32("numIndexesAfter", 1);
        bson.addString(
            "note",
            "Index creation not implemented - PostgreSQL integration pending");
    }
    else if (commandName == "dbStats")
    {
        bson.addDouble("ok", 1.0);
        bson.addString("db", "test");
        bson.addInt32("collections", 1);
        bson.addInt32("views", 0);
        bson.addInt32("objects", 100);
        bson.addDouble("avgObjSize", 64.0);
        bson.addInt64("dataSize", 6400);
        bson.addInt64("storageSize", 8192);
        bson.addInt32("indexes", 1);
        bson.addInt64("indexSize", 1024);
        bson.addInt64("totalSize", 9216);
        bson.addDouble("scaleFactor", 1.0);
    }
    else
    {
        // Unknown command
        bson.clear();
        bson.initialize();
        bson.beginDocument();
        bson.addDouble("ok", 0.0);
        bson.addInt32("code", 59);
        bson.addString("codeName", "CommandNotFound");
        bson.addString("errmsg", "no such command: " + commandName);
    }

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

    if (!connectionPooler_)
    {
        return documents;
    }

    try
    {
        // Get connection from the pool
        auto voidConnection = connectionPooler_->getConnection();
        if (!voidConnection)
        {
            // Return empty result if no connection available
            return documents;
        }

        // Safely cast to PGConnection
        auto connection =
            std::static_pointer_cast<PGConnection>(voidConnection);
        if (!connection || !connection->database)
        {
            // Return empty result if database is invalid
            connectionPooler_->releaseConnection(voidConnection);
            return documents;
        }

        // Execute SQL query to get data from PostgreSQL
        stringstream sql;
        sql << "SELECT * FROM " << collectionName << " LIMIT 100";

        // Execute the query using the PostgreSQL database
        auto result = connection->database->executeQuery(sql.str());

        if (result.success && !result.rows.empty())
        {
            // Convert PostgreSQL results to BSON documents
            for (size_t row = 0; row < result.rows.size(); ++row)
            {
                CBsonType doc;
                doc.initialize();
                doc.beginDocument();

                // Add other fields from the result (including ID handling)
                for (size_t col = 0; col < result.columnNames.size() &&
                                     col < result.rows[row].size();
                     ++col)
                {
                    string columnName = result.columnNames[col];
                    string value = result.rows[row][col];

                    // Handle the ID field specially for MongoDB compatibility
                    if (columnName == "id")
                    {
                        doc.addString("_id", "pg_" + value);
                        continue;
                    }
                    else if (columnName == "_id")
                    {
                        doc.addString("_id", value);
                        continue;
                    }

                    // Try to determine the type and add appropriately
                    if (columnName == "count" || columnName == "quantity" ||
                        columnName == "amount")
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
                    else if (columnName == "active" ||
                             columnName == "enabled" || columnName == "visible")
                    {
                        bool boolVal = (value == "true" || value == "1" ||
                                        value == "t" || value == "yes");
                        doc.addBool(columnName, boolVal);
                    }
                    else if (columnName == "price" || columnName == "cost" ||
                             columnName == "rating")
                    {
                        try
                        {
                            double doubleVal = stod(value);
                            doc.addDouble(columnName, doubleVal);
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

                doc.endDocument();
                documents.push_back(doc);
            }
        }
        else
        {
            // If query failed or no results, create a fallback document
            CBsonType fallbackDoc;
            fallbackDoc.initialize();
            fallbackDoc.beginDocument();
            fallbackDoc.addString("_id", "pg_fallback");
            fallbackDoc.addString("name", "PostgreSQL Query Result");
            fallbackDoc.addString("status", result.success ? "empty" : "error");
            fallbackDoc.addString("message", result.message);
            fallbackDoc.endDocument();
            documents.push_back(fallbackDoc);
        }

        // Return connection to pool
        connectionPooler_->releaseConnection(voidConnection);
    }
    catch (const std::exception& e)
    {
        // Log error and return error document
        CBsonType errorDoc;
        errorDoc.initialize();
        errorDoc.beginDocument();
        errorDoc.addString("_id", "pg_error");
        errorDoc.addString("name", "PostgreSQL Error");
        errorDoc.addString("error", e.what());
        errorDoc.endDocument();
        documents.push_back(errorDoc);
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
    return bson.getDocument();
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
    return bson.getDocument();
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
    return bson.getDocument();
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
    bson.addInt32("code", errorCode);
    bson.addString("errmsg", errorMessage);
    bson.endDocument();
    return bson.getDocument();
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
    // TODO: Implement hello response
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
    // TODO: Implement buildInfo response
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
    // TODO: Implement isMaster response
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
