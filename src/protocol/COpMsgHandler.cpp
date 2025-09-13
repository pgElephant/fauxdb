/*-------------------------------------------------------------------------
 *
 * COpMsgHandler.cpp
 *      OP_MSG message handler for MongoDB wire protocol.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "COpMsgHandler.hpp"

#include "CBsonType.hpp"
#include "CDocumentWireProtocol.hpp"
#include "FindCommand.hpp"
#include "CollectionNameParser.hpp"

#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace std;
namespace FauxDB
{
COpMsgHandler::COpMsgHandler()
    : connectionPooler_(nullptr)
{
}
vector<uint8_t> COpMsgHandler::processMessage(const vector<uint8_t>& message)
{
    try
    {
        OpMsgCommand command = parseMessage(message);
        return routeCommand(command);
    }
    catch (const exception& e)
    {
        return buildErrorResponse(e.what(), 2, "BadValue");
    }
}
OpMsgCommand COpMsgHandler::parseMessage(const vector<uint8_t>& message)
{
    OpMsgCommand command;
    if (message.size() < 20)
        throw runtime_error("Message too short");
    const uint8_t* data = message.data() + 16;
    size_t remaining = message.size() - 16;
    if (remaining < 4)
        throw runtime_error("Cannot read flagBits");
    memcpy(&command.flagBits, data, sizeof(int32_t));
    data += 4;
    remaining -= 4;
    command.checksumPresent =
        (command.flagBits &
         static_cast<int32_t>(CDocumentMsgFlags::CHECKSUM_PRESENT)) != 0;
    if (!parseSections(data, remaining, command.sections))
        throw runtime_error("Failed to parse sections");
    std::cout << "[DEBUG] parseMessage: Parsed " << command.sections.size() << " sections" << std::endl;
    if (!command.sections.empty() &&
        command.sections[0].kind == SectionKind::DOCUMENT)
    {
        std::cout << "[DEBUG] parseMessage: Found document section, parsing command..." << std::endl;
        /* Parse the actual command from the BSON document */
        parseCommandFromSections(command);
    }
    else
    {
        std::cout << "[DEBUG] parseMessage: No document section found" << std::endl;
    }
    return command;
}
vector<uint8_t> COpMsgHandler::handleHello(const OpMsgCommand& command)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();

    /* Standard success response */
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();

    /* Required hello response fields per MongoDB spec */
    if (!bsonBuilder.addBool("isWritablePrimary", true))
        return vector<uint8_t>();
    if (!bsonBuilder.addBool("ismaster",
                             true)) /* Legacy field for compatibility */
        return vector<uint8_t>();
    if (!bsonBuilder.addBool("helloOk",
                             true)) /* Indicates hello command support */
        return vector<uint8_t>();

    /* Wire protocol version - OP_MSG support (MongoDB 3.6) */
    if (!bsonBuilder.addInt32("minWireVersion", 6))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("maxWireVersion", 6))
        return vector<uint8_t>();

    /* Message size limits per MongoDB defaults */
    if (!bsonBuilder.addInt32("maxBsonObjectSize", 16777216)) /* 16MB */
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("maxMessageSizeBytes", 48000000)) /* 48MB */
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("maxWriteBatchSize", 100000)) /* 100k docs */
        return vector<uint8_t>();

    /* Server information */
    if (!bsonBuilder.addString("msg", "FauxDB"))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt64("localTime",
                              1000LL * time(nullptr))) /* Current time in ms */
        return vector<uint8_t>();

    /* Connection information */
    if (!bsonBuilder.addInt32("connectionId", 1))
        return vector<uint8_t>();

    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return buildSuccessResponse(bsonBuilder.getDocument());
}
vector<uint8_t> COpMsgHandler::handlePing(const OpMsgCommand& command)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return buildSuccessResponse(bsonBuilder.getDocument());
}
vector<uint8_t> COpMsgHandler::handleBuildInfo(const OpMsgCommand& command)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("version", "1.0.0"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("gitVersion", "fauxdb-1.0.0"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("modules", "none"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("allocator", "system"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("javascriptEngine", "none"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("sysInfo", "FauxDB Server"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("versionArray", "1.0.0"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("openssl", "OpenSSL 3.0.0"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("buildEnvironment", "fauxdb-x86_64-clang"))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return buildSuccessResponse(bsonBuilder.getDocument());
}
vector<uint8_t> COpMsgHandler::handleGetParameter(const OpMsgCommand& command)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("featureCompatibilityVersion", "7.0"))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return buildSuccessResponse(bsonBuilder.getDocument());
}
vector<uint8_t> COpMsgHandler::handleFind(const OpMsgCommand& command)
{
    std::cout << "[DEBUG] COpMsgHandler::handleFind: Starting find command" << std::endl;
    
    /* Extract collection name from command using CollectionNameParser */
    std::cout << "[DEBUG] COpMsgHandler::handleFind: Extracting collection name..." << std::endl;
    string collection = CollectionNameParser::extractCollectionName(
        command.commandBody, command.commandBody.size(), command.commandName);
    
    std::cout << "[DEBUG] COpMsgHandler::handleFind: Collection name: '" << collection << "'" << std::endl;
    
    /* If no collection name found, default to users */
    if (collection.empty())
    {
        collection = "users";
        std::cout << "[DEBUG] COpMsgHandler::handleFind: Using default collection: 'users'" << std::endl;
    }
    
    /* Use FindCommand to execute the query */
    std::cout << "[DEBUG] COpMsgHandler::handleFind: Creating FindCommand..." << std::endl;
    FindCommand findCmd;
    
    std::cout << "[DEBUG] COpMsgHandler::handleFind: Calling FindCommand::execute..." << std::endl;
    auto result = findCmd.execute(collection, command.commandBody, command.commandBody.size(), connectionPooler_);
    
    std::cout << "[DEBUG] COpMsgHandler::handleFind: FindCommand::execute completed, result size: " << result.size() << std::endl;
    return result;
}
vector<uint8_t> COpMsgHandler::handleInsert(const OpMsgCommand& command)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("n", 0))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return bsonBuilder.getDocument();
}
vector<uint8_t> COpMsgHandler::handleUpdate(const OpMsgCommand& command)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("n", 0))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("nModified", 0))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return bsonBuilder.getDocument();
}
vector<uint8_t> COpMsgHandler::handleDelete(const OpMsgCommand& command)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("n", 0))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return bsonBuilder.getDocument();
}
vector<uint8_t> COpMsgHandler::handleGetMore(const OpMsgCommand& command)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("nextBatch", "[]"))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return bsonBuilder.getDocument();
}
vector<uint8_t> COpMsgHandler::handleKillCursors(const OpMsgCommand& command)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("cursorsKilled", "0"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("cursorsNotFound", "0"))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("cursorsAlive", "0"))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return bsonBuilder.getDocument();
}
vector<uint8_t>
COpMsgHandler::buildSuccessResponse(const vector<uint8_t>& response)
{
    return response;
}
vector<uint8_t> COpMsgHandler::buildErrorResponse(const string& error, int code,
                                                  const string& codeName)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 0.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("code", code))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("codeName", codeName))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("errmsg", error))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return bsonBuilder.getDocument();
}
vector<uint8_t>
COpMsgHandler::buildCursorResponse(const string& ns, int64_t cursorId,
                                   const vector<vector<uint8_t>>& batch)
{
    FauxDB::CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt64("cursorId", cursorId))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("namespace", ns))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("firstBatch", "[]"))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    return bsonBuilder.getDocument();
}
bool COpMsgHandler::parseSections(const uint8_t*& data, size_t& remaining,
                                  vector<OpMsgSection>& sections)
{
    while (remaining > 0)
    {
        if (remaining < 1)
            return false;
        uint8_t kind = *data;
        data++;
        remaining--;
        if (kind == static_cast<uint8_t>(SectionKind::DOCUMENT))
            sections.push_back(parseDocumentSection(data, remaining));
        else if (kind == static_cast<uint8_t>(SectionKind::DOCUMENT_SEQUENCE))
            sections.push_back(parseDocumentSequenceSection(data, remaining));
        else
            return false;
    }
    return true;
}
OpMsgSection COpMsgHandler::parseDocumentSection(const uint8_t*& data,
                                                 size_t& remaining)
{
    OpMsgSection section;
    section.kind = SectionKind::DOCUMENT;
    if (remaining >= 4)
    {
        int32_t docSize;
        memcpy(&docSize, data, sizeof(int32_t));
        if (docSize > 0 && static_cast<size_t>(docSize) <= remaining)
        {
            data += docSize;
            remaining -= docSize;
        }
    }
    return section;
}
OpMsgSection COpMsgHandler::parseDocumentSequenceSection(const uint8_t*& data,
                                                         size_t& remaining)
{
    OpMsgSection section;
    section.kind = SectionKind::DOCUMENT_SEQUENCE;
    return section;
}
vector<uint8_t> COpMsgHandler::routeCommand(const OpMsgCommand& command)
{
    std::cout << "[DEBUG] routeCommand: Routing command '" << command.commandName << "' to database '" << command.database << "'" << std::endl;
    if (command.commandName == "hello" || command.commandName == "isMaster")
        return handleHello(command);
    else if (command.commandName == "ping")
        return handlePing(command);
    else if (command.commandName == "buildInfo")
        return handleBuildInfo(command);
    else if (command.commandName == "getParameter")
        return handleGetParameter(command);
    else if (command.commandName == "find")
        return handleFind(command);
    else if (command.commandName == "insert")
        return handleInsert(command);
    else if (command.commandName == "update")
        return handleUpdate(command);
    else if (command.commandName == "delete")
        return handleDelete(command);
    else if (command.commandName == "getMore")
        return handleGetMore(command);
    else if (command.commandName == "killCursors")
        return handleKillCursors(command);
    else
        return buildErrorResponse("no such command", 59, "CommandNotFound");
}
bool COpMsgHandler::validateCommand(const OpMsgCommand& command)
{
    return !command.database.empty() && !command.commandName.empty();
}
bool COpMsgHandler::validateChecksum(const vector<uint8_t>& message,
                                     const OpMsgCommand& command)
{
    if (!command.checksumPresent)
        return true;
    return true;
}
uint32_t COpMsgHandler::calculateCRC32C(const uint8_t* data, size_t size)
{
    return 0;
}
vector<uint8_t> COpMsgHandler::serializeBsonDocument(const vector<uint8_t>& doc)
{
    return doc;
}

void COpMsgHandler::setConnectionPooler(std::shared_ptr<CPGConnectionPooler> pooler)
{
    connectionPooler_ = pooler;
}

void COpMsgHandler::parseCommandFromSections(OpMsgCommand& command)
{
    std::cout << "[DEBUG] parseCommandFromSections: Starting command parsing" << std::endl;
    
    /* Default values */
    command.database = "admin";
    command.commandName = "hello";
    command.commandBody.clear();
    
    /* Parse the first document section to extract command name and database */
    std::cout << "[DEBUG] parseCommandFromSections: Sections count: " << command.sections.size() << std::endl;
    if (!command.sections.empty() && 
        command.sections[0].kind == SectionKind::DOCUMENT &&
        !command.sections[0].documents.empty())
    {
        const auto& bsonDoc = command.sections[0].documents[0];
        command.commandBody = bsonDoc;
        std::cout << "[DEBUG] parseCommandFromSections: BSON document size: " << bsonDoc.size() << std::endl;
        
        /* Parse BSON to extract command name and database */
        if (bsonDoc.size() > 4)
        {
            size_t offset = 4; /* Skip document size */
            
            while (offset < bsonDoc.size())
            {
                if (offset >= bsonDoc.size()) break;
                
                uint8_t type = bsonDoc[offset++];
                if (type == 0x00) break; /* End of document */
                
                /* Read key name */
                size_t keyStart = offset;
                while (offset < bsonDoc.size() && bsonDoc[offset] != 0x00)
                    offset++;
                if (offset >= bsonDoc.size()) break;
                
                string key(reinterpret_cast<const char*>(&bsonDoc[keyStart]), 
                          offset - keyStart);
                offset++; /* Skip null terminator */
                
                /* Handle different field types */
                if (type == 0x02) /* String */
                {
                    if (offset + 4 >= bsonDoc.size()) break;
                    
                    int32_t strLen;
                    memcpy(&strLen, &bsonDoc[offset], sizeof(int32_t));
                    offset += 4;
                    
                    if (offset + strLen - 1 >= bsonDoc.size()) break;
                    
                    string value(reinterpret_cast<const char*>(&bsonDoc[offset]), 
                                strLen - 1); /* -1 to exclude null terminator */
                    offset += strLen;
                    
                    if (key == "$db")
                    {
                        command.database = value;
                        std::cout << "[DEBUG] parseCommandFromSections: Found database: " << value << std::endl;
                    }
                    else if (key != "$db" && command.commandName == "hello")
                    {
                        /* First non-$db field is the command name */
                        command.commandName = key;
                        std::cout << "[DEBUG] parseCommandFromSections: Found command: " << key << std::endl;
                    }
                }
                else if (type == 0x01) /* Double */
                {
                    if (offset + 8 > bsonDoc.size()) break;
                    offset += 8;
                }
                else if (type == 0x08) /* Boolean */
                {
                    if (offset >= bsonDoc.size()) break;
                    offset++;
                }
                else if (type == 0x10) /* Int32 */
                {
                    if (offset + 4 > bsonDoc.size()) break;
                    offset += 4;
                }
                else if (type == 0x12) /* Int64 */
                {
                    if (offset + 8 > bsonDoc.size()) break;
                    offset += 8;
                }
                else if (type == 0x05) /* Binary */
                {
                    if (offset + 5 >= bsonDoc.size()) break;
                    int32_t binLen;
                    memcpy(&binLen, &bsonDoc[offset], sizeof(int32_t));
                    offset += 4 + 1 + binLen; /* subtype + data */
                }
                else if (type == 0x03 || type == 0x04) /* Document or Array */
                {
                    if (offset + 4 >= bsonDoc.size()) break;
                    int32_t docLen;
                    memcpy(&docLen, &bsonDoc[offset], sizeof(int32_t));
                    offset += docLen;
                }
                else
                {
                    /* Skip unknown types */
                    break;
                }
            }
        }
    }
}
} // namespace FauxDB
