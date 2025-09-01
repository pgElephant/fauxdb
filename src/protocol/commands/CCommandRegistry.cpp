/*-------------------------------------------------------------------------
 *
 * CCommandRegistry.cpp
 *      Registry implementation for managing document database commands
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CCommandRegistry.hpp"

namespace FauxDB
{


CCommandRegistry::CCommandRegistry()
{
    initializeBuiltinCommands();
}


CCommandRegistry::~CCommandRegistry()
{
    /* Unique pointers will clean up automatically */
}


void
CCommandRegistry::registerCommand(unique_ptr<IDocumentCommand> command)
{
    if (command)
    {
        string commandName = command->getCommandName();
        commands_[commandName] = std::move(command);
    }
}


void
CCommandRegistry::unregisterCommand(const string& commandName)
{
    auto it = commands_.find(commandName);
    if (it != commands_.end())
    {
        commands_.erase(it);
    }
}


bool
CCommandRegistry::hasCommand(const string& commandName) const
{
    return commands_.find(commandName) != commands_.end();
}


vector<uint8_t>
CCommandRegistry::executeCommand(const string& commandName, const CommandContext& context)
{
    auto it = commands_.find(commandName);
    if (it != commands_.end())
    {
        return it->second->execute(context);
    }
    
    /* Return command not found error */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 0.0);
    bson.addInt32("code", 59);
    bson.addString("errmsg", "no such command: '" + commandName + "'");
    bson.endDocument();
    return bson.getDocument();
}


vector<string>
CCommandRegistry::getRegisteredCommands() const
{
    vector<string> commandNames;
    commandNames.reserve(commands_.size());
    
    for (const auto& pair : commands_)
    {
        commandNames.push_back(pair.first);
    }
    
    return commandNames;
}


size_t
CCommandRegistry::getCommandCount() const
{
    return commands_.size();
}


void
CCommandRegistry::initializeBuiltinCommands()
{
    /* Built-in commands will be registered here */
    /* For now, we'll register them from the main protocol handler */
}

} // namespace FauxDB
