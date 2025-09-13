/*-------------------------------------------------------------------------
 *
 * CDocumentProtocolHandler.hpp
 *      Document Protocol Handler for FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "../database/CPGConnectionPooler.hpp"
#include "CBsonType.hpp"
#include "CDocumentWireProtocol.hpp"
#include "CResponseBuilder.hpp"
#include "IInterfaces.hpp"
#include "commands/CCommandRegistry.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace FauxDB
{

using std::function;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

// Forward declarations

// CRUD operation structures - simplified without CBsonType objects
struct UpdateOperation
{
    string filterJson;
    string updateJson;
};

struct DeleteOperation
{
    string filterJson;
};

class IDocumentCommandHandler
{
  public:
    virtual ~IDocumentCommandHandler() = default;

    virtual unique_ptr<CDocumentWireMessage>
    handleCommand(const CDocumentWireMessage& request) = 0;
    virtual vector<string> getSupportedCommands() const = 0;
    virtual bool isCommandSupported(const string& command) const = 0;
};

class CDocumentProtocolHandler
{
    // Helper for lazy pooler initialization
    void ensurePostgresPoolerInitialized();

  public:
    CDocumentProtocolHandler();
    ~CDocumentProtocolHandler();

    bool initialize();
    void shutdown();
    bool isInitialized() const;

    unique_ptr<CDocumentWireMessage>
    processMessage(const vector<uint8_t>& messageData);

    /* Protocol stub methods */
    bool start();
    void stop();
    bool isRunning() const;
    std::vector<std::string> getSupportedCommands() const;
    vector<uint8_t> processDocumentMessage(const vector<uint8_t>& messageData,
                                           ssize_t bytesRead,

                                           CResponseBuilder& responseBuilder);

    void registerCommandHandler(const string& command,
                                unique_ptr<IDocumentCommandHandler> handler);
    void unregisterCommandHandler(const string& command);

    /* Set connection pooler for PostgreSQL access */
    void setConnectionPooler(shared_ptr<CPGConnectionPooler> pooler);

    /* Set logger for debugging and error reporting */
    void setLogger(shared_ptr<ILogger> logger);

  private:
    bool initialized_;
    bool isRunning_;
    size_t maxBsonSize_;
    bool compressionEnabled_;
    bool checksumEnabled_;
    size_t messageCount_;
    size_t errorCount_;
    size_t compressedMessageCount_;
    unordered_map<string, unique_ptr<IDocumentCommandHandler>> commandHandlers_;
    unique_ptr<CDocumentWireParser> wireParser_;
    unique_ptr<CDocumentWireParser> parser_;

    unique_ptr<CResponseBuilder> responseBuilder_;
    shared_ptr<CPGConnectionPooler> connectionPooler_;
    unique_ptr<CCommandRegistry> commandRegistry_;
    shared_ptr<ILogger> logger_;

    bool parseMessage(const vector<uint8_t>& messageData,
                      CDocumentWireMessage& message);
    unique_ptr<CDocumentWireMessage>
    executeCommand(const string& command, const CDocumentWireMessage& request);

    void initializeConfiguration();
    void initializeDefaultCommandHandlers();
    vector<uint8_t> createErrorBsonDocument(int errorCode,
                                            const string& errorMessage);
    string extractCommandName(const CDocumentWireMessage& request);
    string extractCollectionName(const vector<uint8_t>& buffer,
                                 ssize_t bytesRead);

    // Additional methods referenced in implementation
    unique_ptr<CDocumentWireMessage>
    routeCommand(const CDocumentWireMessage& request);
    unique_ptr<CDocumentWireMessage> createHelloResponse(int32_t requestID);
    unique_ptr<CDocumentWireMessage> createBuildInfoResponse(int32_t requestID);
    unique_ptr<CDocumentWireMessage> createIsMasterResponse(int32_t requestID);
    unique_ptr<CDocumentWireMessage>
    createErrorResponse(int32_t requestID, int32_t errorCode,
                        const string& errorMessage);
    vector<uint8_t>
    createBsonDocument(const unordered_map<string, string>& fields);
    vector<uint8_t>
    createBsonDocument(const unordered_map<string, int32_t>& fields);
    vector<uint8_t>
    createBsonDocument(const unordered_map<string, double>& fields);
    vector<uint8_t>
    createBsonDocument(const unordered_map<string, bool>& fields);

    /* Helper methods for PostgreSQL queries */
    vector<uint8_t>
    createFindResponseFromPostgreSQL(const string& collectionName,
                                     int32_t requestID);
    vector<uint8_t>
    createCountResponseFromPostgreSQL(const string& collectionName,
                                      int32_t requestID);
    vector<CBsonType> queryPostgreSQLCollection(const string& collectionName);

    /* CRUD operation helper methods */
    vector<CBsonType> extractDocumentsFromInsert(const vector<uint8_t>& buffer,
                                                 ssize_t bytesRead);
    vector<UpdateOperation>
    extractUpdateOperations(const vector<uint8_t>& buffer, ssize_t bytesRead);
    vector<DeleteOperation>
    extractDeleteOperations(const vector<uint8_t>& buffer, ssize_t bytesRead);
    string convertBsonToInsertSQL(const string& collectionName,
                                  const CBsonType& doc);
    string convertUpdateToSQL(const string& collectionName,
                              const string& filterJson,
                              const string& updateJson);
    string convertDeleteToSQL(const string& collectionName,
                              const string& filterJson);

    /* Wire protocol helper */
    std::vector<uint8_t> createWireMessage(int32_t requestID,
                                           int32_t responseTo,
                                           const std::vector<uint8_t>& bsonDoc);
    std::vector<uint8_t> createSimpleOkBson();

    /* Command-specific response creators for OP_MSG */
    std::vector<uint8_t> createHelloWireResponse(int32_t requestID);
    std::vector<uint8_t> createPingWireResponse(int32_t requestID);
    std::vector<uint8_t> createListDatabasesWireResponse(int32_t requestID);
    std::vector<uint8_t> createFindWireResponse(int32_t requestID);
    std::vector<uint8_t> createInsertWireResponse(int32_t requestID);
    std::vector<uint8_t> createBuildInfoWireResponse(int32_t requestID);
    std::vector<uint8_t> createAggregateWireResponse(int32_t requestID);
    std::vector<uint8_t> createAtlasVersionWireResponse(int32_t requestID);
    std::vector<uint8_t> createGetParameterWireResponse(int32_t requestID);
    std::vector<uint8_t> createErrorWireResponse(int32_t requestID = 0);

    /* Command-specific response creators for OP_REPLY (legacy) */
    std::vector<uint8_t> createHelloOpReplyResponse(int32_t requestID);
    std::vector<uint8_t> createPingOpReplyResponse(int32_t requestID);
    std::vector<uint8_t> createErrorOpReplyResponse(int32_t requestID);
    std::vector<uint8_t>
    createFindOpReplyResponseFromPostgreSQL(const std::string& collectionName,
                                            int32_t requestID);
    std::vector<uint8_t>
    createOpReplyResponse(int32_t requestID,
                          const std::vector<uint8_t>& bsonDocument);

    /* BSON parsing helper */
    std::string parseCommandFromBSON(const std::vector<uint8_t>& buffer,
                                     size_t offset, size_t remaining);
    std::string parseCollectionNameFromBSON(const std::vector<uint8_t>& buffer,
                                            size_t offset, size_t remaining, const std::string& commandName);
};

class CHelloCommandHandler : public IDocumentCommandHandler
{
  public:
    CHelloCommandHandler();
    ~CHelloCommandHandler();

    unique_ptr<CDocumentWireMessage>
    handleCommand(const CDocumentWireMessage& request) override;
    vector<string> getSupportedCommands() const override;
    bool isCommandSupported(const string& command) const override;

  private:
    vector<uint8_t> createHelloResponseDocument();
};

class CBuildInfoCommandHandler : public IDocumentCommandHandler
{
  public:
    CBuildInfoCommandHandler();
    ~CBuildInfoCommandHandler();

    unique_ptr<CDocumentWireMessage>
    handleCommand(const CDocumentWireMessage& request) override;
    vector<string> getSupportedCommands() const override;
    bool isCommandSupported(const string& command) const override;

  private:
    vector<uint8_t> createBuildInfoResponseDocument();
};

class CIsMasterCommandHandler : public IDocumentCommandHandler
{
  public:
    CIsMasterCommandHandler();
    ~CIsMasterCommandHandler();

    unique_ptr<CDocumentWireMessage>
    handleCommand(const CDocumentWireMessage& request) override;
    vector<string> getSupportedCommands() const override;
    bool isCommandSupported(const string& command) const override;

  private:
    vector<uint8_t> createIsMasterResponseDocument();
};

} /* namespace FauxDB */
