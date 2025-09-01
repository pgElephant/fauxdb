/* MongoDB Command Registry Implementation */
#include "protocol/CommandRegistry.hpp"

#include "protocol/CountCommand.hpp"
#include "protocol/FindCommand.hpp"
#include "protocol/FindOneCommand.hpp"

namespace FauxDB
{

CommandRegistry::CommandRegistry()
{
    registerBuiltinCommands();
}

void CommandRegistry::registerBuiltinCommands()
{
    /* Register find commands */
    registerCommand("find", make_shared<FindCommand>());
    registerCommand("findOne", make_shared<FindOneCommand>());

    /* Register count commands */
    registerCommand("countDocuments", make_shared<CountDocumentsCommand>());
    registerCommand(
        "count",
        make_shared<CountDocumentsCommand>()); /* Alias for countDocuments */
    registerCommand("estimatedDocumentCount",
                    make_shared<EstimatedDocumentCountCommand>());
}

void CommandRegistry::registerCommand(const string& commandName,
                                      CommandPtr command)
{
    commands_[commandName] = command;
}

CommandPtr CommandRegistry::getCommand(const string& commandName) const
{
    auto it = commands_.find(commandName);
    if (it != commands_.end())
    {
        return it->second;
    }
    return nullptr;
}

bool CommandRegistry::hasCommand(const string& commandName) const
{
    return commands_.find(commandName) != commands_.end();
}

vector<string> CommandRegistry::getCommandNames() const
{
    vector<string> names;
    names.reserve(commands_.size());

    for (const auto& pair : commands_)
    {
        names.push_back(pair.first);
    }

    return names;
}

} /* namespace FauxDB */
