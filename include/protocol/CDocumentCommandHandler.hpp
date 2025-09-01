/*-------------------------------------------------------------------------
 *
 * CDocumentCommandHandler.hpp
 *      Document document command handler for FauxDB.
 *      Handles Document-specific commands and operations.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CCommandHandler.hpp"
#include "CDocumentProtocolHandler.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace FauxDB
{

class CDocumentCommandHandler : public CCommandHandler,
                                public IDocumentCommandHandler
{
  public:
    CDocumentCommandHandler();
    virtual ~CDocumentCommandHandler();

    /* CCommandHandler interface implementation */
    CCommandResult
    handleCommand(const std::string& commandName,
                  const std::vector<uint8_t>& commandData) override;
    bool supportsCommand(const std::string& commandName) const override;
    bool validateCommand(const std::string& commandName,
                         const std::vector<uint8_t>& commandData) override;
    std::string getCommandHelp(const std::string& commandName) const override;

    /* IDocumentCommandHandler interface implementation */
    std::unique_ptr<CDocumentWireMessage>
    handleCommand(const CDocumentWireMessage& request) override;
    std::vector<std::string> getSupportedCommands() const override;
    bool isCommandSupported(const std::string& command) const override;

    /* Document-specific command handlers */
    CCommandResult handleHello(const std::vector<uint8_t>& commandData);
    CCommandResult handlePing(const std::vector<uint8_t>& commandData);
    CCommandResult handleBuildInfo(const std::vector<uint8_t>& commandData);
    CCommandResult handleGetParameter(const std::vector<uint8_t>& commandData);
    CCommandResult handleFind(const std::vector<uint8_t>& commandData);
    CCommandResult handleAggregate(const std::vector<uint8_t>& commandData);
    CCommandResult handleInsert(const std::vector<uint8_t>& commandData);
    CCommandResult handleUpdate(const std::vector<uint8_t>& commandData);
    CCommandResult handleDelete(const std::vector<uint8_t>& commandData);
    CCommandResult handleCount(const std::vector<uint8_t>& commandData);
    CCommandResult handleDistinct(const std::vector<uint8_t>& commandData);
    CCommandResult handleCreateIndex(const std::vector<uint8_t>& commandData);
    CCommandResult handleDropIndex(const std::vector<uint8_t>& commandData);
    CCommandResult handleListIndexes(const std::vector<uint8_t>& commandData);
    CCommandResult
    handleListCollections(const std::vector<uint8_t>& commandData);
    CCommandResult handleListDatabases(const std::vector<uint8_t>& commandData);

    /* Command configuration */
    void setDatabase(const std::string& database);
    void setCollection(const std::string& collection);
    void setMaxBSONSize(size_t maxSize);
    void setMaxMessageSize(size_t maxSize);

  protected:
    /* CCommandHandler interface implementation */
    void initializeDefaultCommands() override;

  private:
    /* Configuration */
    std::string currentDatabase_;
    std::string currentCollection_;
    size_t maxBSONSize_;
    size_t maxMessageSize_;
    std::vector<std::string> supportedCommands_;

    /* Private methods */
    CCommandResult parseCommandData(const std::vector<uint8_t>& commandData);
    std::string extractCommandName(const std::vector<uint8_t>& commandData);
    std::vector<uint8_t>
    extractCommandArguments(const std::vector<uint8_t>& commandData);

    /* Command validation */
    bool validateBSONSize(const std::vector<uint8_t>& data);
    bool validateMessageSize(const std::vector<uint8_t>& data);
    bool validateCommandFormat(const std::vector<uint8_t>& data);

    /* Response building helpers */
    CCommandResult buildHelloResponse();
    CCommandResult buildBuildInfoResponse();
    CCommandResult buildGetParameterResponse();
    CCommandResult buildFindResponse(const std::vector<uint8_t>& queryData);
    CCommandResult
    buildAggregateResponse(const std::vector<uint8_t>& pipelineData);

    /* Utility methods */
    std::string getCurrentDatabase() const;
    std::string getCurrentCollection() const;
    bool isAdminCommand(const std::string& commandName) const;
    bool isReadCommand(const std::string& commandName) const;
    bool isWriteCommand(const std::string& commandName) const;
};

} /* namespace FauxDB */
