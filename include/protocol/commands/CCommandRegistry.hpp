/*-------------------------------------------------------------------------
 *
 * CCommandRegistry.hpp
 *      Registry for managing document database commands
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "IDocumentCommand.hpp"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

namespace FauxDB
{

using std::unique_ptr;
using std::unordered_map;
using std::string;
using std::vector;

class CCommandRegistry
{
public:
    CCommandRegistry();
    ~CCommandRegistry();
    
    /* Command registration */
    void registerCommand(unique_ptr<IDocumentCommand> command);
    void unregisterCommand(const string& commandName);
    
    /* Command execution */
    bool hasCommand(const string& commandName) const;
    vector<uint8_t> executeCommand(const string& commandName, const CommandContext& context);
    
    /* Registry information */
    vector<string> getRegisteredCommands() const;
    size_t getCommandCount() const;

private:
    unordered_map<string, unique_ptr<IDocumentCommand>> commands_;
    
    void initializeBuiltinCommands();
};

} // namespace FauxDB
