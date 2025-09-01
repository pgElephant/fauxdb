/* MongoDB Command Registry */
#pragma once

#include "ICommand.hpp"
#include <unordered_map>
#include <memory>

namespace FauxDB
{

/**
 * @brief Registry for MongoDB commands
 */
class CommandRegistry
{
private:
    unordered_map<string, CommandPtr> commands_;
    
    /**
     * @brief Register all built-in commands
     */
    void registerBuiltinCommands();

public:
    /**
     * @brief Constructor - registers all built-in commands
     */
    CommandRegistry();
    
    /**
     * @brief Register a command
     * @param commandName Command name
     * @param command Command implementation
     */
    void registerCommand(const string& commandName, CommandPtr command);
    
    /**
     * @brief Get command by name
     * @param commandName Command name
     * @return Command implementation or nullptr if not found
     */
    CommandPtr getCommand(const string& commandName) const;
    
    /**
     * @brief Check if command exists
     * @param commandName Command name
     * @return true if command exists, false otherwise
     */
    bool hasCommand(const string& commandName) const;
    
    /**
     * @brief Get list of all registered command names
     * @return Vector of command names
     */
    vector<string> getCommandNames() const;
};

} /* namespace FauxDB */
