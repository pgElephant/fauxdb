/*-------------------------------------------------------------------------
 *
 * CCommandHandler.cpp
 *      Base command handler implementation for FauxDB.
 *      Abstract base class for handling different types of commands.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "CCommandHandler.hpp"

#include <algorithm>
#include <stdexcept>

using namespace FauxDB;

/*-------------------------------------------------------------------------
 * CCommandResult implementation
 *-------------------------------------------------------------------------*/

CCommandResult::CCommandResult() : success(false), errorCode(0)
{
}

CCommandResult::CCommandResult(bool s) : success(s), errorCode(0)
{
}

CCommandResult::CCommandResult(bool s, const std::vector<uint8_t>& resp)
    : success(s), response(resp), errorCode(0)
{
}

CCommandResult::CCommandResult(bool s, const std::string& error, int32_t code)
    : success(s), errorMessage(error), errorCode(code)
{
}

CCommandResult
CCommandResult::createSuccess(const std::vector<uint8_t>& response)
{
    return CCommandResult(true, response);
}

CCommandResult CCommandResult::createError(const std::string& message,
                                           int32_t code)
{
    return CCommandResult(false, message, code);
}

CCommandResult CCommandResult::createError(int32_t code,
                                           const std::string& message)
{
    return CCommandResult(false, message, code);
}

/*-------------------------------------------------------------------------
 * CCommandHandler implementation
 *-------------------------------------------------------------------------*/

CCommandHandler::CCommandHandler()
{
}

CCommandHandler::~CCommandHandler()
{
    commandHandlers_.clear();
}

void CCommandHandler::registerCommand(
    const std::string& name,
    std::function<CCommandResult(const std::vector<uint8_t>&)> handler)
{
    if (name.empty())
    {
        throw std::invalid_argument("Command name cannot be empty");
    }
    commandHandlers_[name] = handler;
}

void CCommandHandler::unregisterCommand(const std::string& name)
{
    auto it = commandHandlers_.find(name);
    if (it != commandHandlers_.end())
    {
        commandHandlers_.erase(it);
    }
}

CCommandResult
CCommandHandler::routeCommand(const std::string& commandName,
                              const std::vector<uint8_t>& commandData)
{
    auto it = commandHandlers_.find(commandName);
    if (it == commandHandlers_.end())
    {
        return CCommandResult::createError(
            "Unsupported command: " + commandName, 59);
    }
    try
    {
        return it->second(commandData);
    }
    catch (const std::exception& e)
    {
        return CCommandResult::createError(
            "Command execution failed: " + std::string(e.what()), 1);
    }
}

std::vector<std::string> CCommandHandler::getSupportedCommands() const
{
    std::vector<std::string> commands;
    commands.reserve(commandHandlers_.size());
    for (const auto& pair : commandHandlers_)
    {
        commands.push_back(pair.first);
    }
    return commands;
}

size_t CCommandHandler::getCommandCount() const
{
    return commandHandlers_.size();
}

CCommandResult CCommandHandler::executeCommand(const std::string& name,
                                               const std::vector<uint8_t>& data)
{
    auto it = commandHandlers_.find(name);
    if (it == commandHandlers_.end())
    {
        return CCommandResult::createError("Command not found: " + name, 59);
    }
    try
    {
        return it->second(data);
    }
    catch (const std::exception& e)
    {
        return CCommandResult::createError(
            "Command execution failed: " + std::string(e.what()), 1);
    }
}

CCommandResult CCommandHandler::buildErrorResponse(int32_t code,
                                                   const std::string& codeName,
                                                   const std::string& message)
{
    return CCommandResult::createError(message, code);
}

CCommandResult
CCommandHandler::buildSuccessResponse(const std::vector<uint8_t>& data)
{
    return CCommandResult::createSuccess(data);
}

/* End of file */
