/*-------------------------------------------------------------------------
 *
 * CCommandHandler.hpp
 *      Base command handler for FauxDB.
 *      Abstract base class for handling different types of commands.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#pragma once
#include "CBsonType.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
namespace FauxDB
{

using namespace FauxDB;

/* Forward declarations */
struct CCommandResult;

/**
 * Command result structure
 */
struct CCommandResult
{
    bool success;
    std::vector<uint8_t> response;
    std::string errorMessage;
    int32_t errorCode;

    CCommandResult();
    CCommandResult(bool s);
    CCommandResult(bool s, const std::vector<uint8_t>& resp);
    CCommandResult(bool s, const std::string& error, int32_t code);

    /* Utility methods */
    static CCommandResult createSuccess(const std::vector<uint8_t>& response);
    static CCommandResult createError(const std::string& message,
                                      int32_t code = 0);
    static CCommandResult createError(int32_t code, const std::string& message);
};

/**
 * Base command handler class
 * Abstract base class for handling different types of commands
 */
class CCommandHandler
{
  public:
    CCommandHandler();
    virtual ~CCommandHandler();
    // ...existing code...

    /* Command handling */
    virtual CCommandResult
    handleCommand(const std::string& commandName,
                  const std::vector<uint8_t>& commandData) = 0;
    virtual bool supportsCommand(const std::string& commandName) const = 0;

    /* Command registration */
    void registerCommand(
        const std::string& name,
        std::function<CCommandResult(const std::vector<uint8_t>&)> handler);
    void unregisterCommand(const std::string& name);

    /* Command routing */
    CCommandResult routeCommand(const std::string& commandName,
                                const std::vector<uint8_t>& commandData);

    /* Command validation */
    virtual bool validateCommand(const std::string& commandName,
                                 const std::vector<uint8_t>& commandData) = 0;
    virtual std::string
    getCommandHelp(const std::string& commandName) const = 0;

    /* Command information */
    std::vector<std::string> getSupportedCommands() const;
    size_t getCommandCount() const;

  protected:
    /* Command registry */
    std::unordered_map<
        std::string, std::function<CCommandResult(const std::vector<uint8_t>&)>>
        commandHandlers_;

    /* Utility methods */
    virtual void initializeDefaultCommands() = 0;
    CCommandResult executeCommand(const std::string& name,
                                  const std::vector<uint8_t>& data);

    /* Error handling */
    CCommandResult buildErrorResponse(int32_t code, const std::string& codeName,
                                      const std::string& message);
    CCommandResult buildSuccessResponse(const std::vector<uint8_t>& data);
};

} /* namespace FauxDB */
