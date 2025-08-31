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
using namespace std;
using namespace FauxDB;

// FauxDB namespace removed for direct symbol usage

/*-------------------------------------------------------------------------
 * CCommandResult implementation
 *-------------------------------------------------------------------------*/

CCommandResult::CCommandResult() : success(false), errorCode(0)
{
}

CCommandResult::CCommandResult(bool s) : success(s), errorCode(0)
{
}

CCommandResult::CCommandResult(bool s, const vector<uint8_t>& resp)
	: success(s), response(resp), errorCode(0)
{
}

CCommandResult::CCommandResult(bool s, const string& error, int32_t code)
	: success(s), errorMessage(error), errorCode(code)
{
}

CCommandResult CCommandResult::createSuccess(const vector<uint8_t>& response)
{
	return CCommandResult(true, response);
}

CCommandResult CCommandResult::createError(const string& message, int32_t code)
{
	return CCommandResult(false, message, code);
}

CCommandResult CCommandResult::createError(int32_t code, const string& message)
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
	// TODO: Implement all command handler logic. All current methods are stubs or placeholders.
	//       Replace with real implementations as protocol details are finalized.

void CCommandHandler::registerCommand(
	const string& name,
	function<CCommandResult(const vector<uint8_t>&)> handler)
{
	if (name.empty())
		throw invalid_argument("Command name cannot be empty");
	commandHandlers_[name] = handler;
}

void CCommandHandler::unregisterCommand(const string& name)
{
	auto it = commandHandlers_.find(name);
	if (it != commandHandlers_.end())
		commandHandlers_.erase(it);
}

CCommandResult CCommandHandler::routeCommand(const string& commandName,
											 const vector<uint8_t>& commandData)
{
	auto it = commandHandlers_.find(commandName);
	if (it == commandHandlers_.end())
		return CCommandResult::createError(
			"Unsupported command: " + commandName, 59);
	try
	{
		return it->second(commandData);
	}
	catch (const exception& e)
	{
		return CCommandResult::createError(
			"Command execution failed: " + string(e.what()), 1);
	}
}

vector<string> CCommandHandler::getSupportedCommands() const
{
	vector<string> commands;
	commands.reserve(commandHandlers_.size());
	for (const auto& pair : commandHandlers_)
		commands.push_back(pair.first);
	return commands;
}

size_t CCommandHandler::getCommandCount() const
{
	return commandHandlers_.size();
}

CCommandResult CCommandHandler::executeCommand(const string& name,
											   const vector<uint8_t>& data)
{
	auto it = commandHandlers_.find(name);
	if (it == commandHandlers_.end())
		return CCommandResult::createError("Command not found: " + name, 59);
	try
	{
		return it->second(data);
	}
	catch (const exception& e)
	{
		return CCommandResult::createError(
			"Command execution failed: " + string(e.what()), 1);
	}
}

CCommandResult CCommandHandler::buildErrorResponse(int32_t code,
												   const string& codeName,
												   const string& message)
{
	return CCommandResult::createError(message, code);
}

CCommandResult
CCommandHandler::buildSuccessResponse(const vector<uint8_t>& data)
{
	return CCommandResult::createSuccess(data);
}

// End of file
