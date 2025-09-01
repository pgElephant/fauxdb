/*-------------------------------------------------------------------------
 *
 * CPingCommand.cpp
 *      Ping command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CPingCommand.hpp"

namespace FauxDB
{

CPingCommand::CPingCommand()
{
    /* Constructor */
}

string CPingCommand::getCommandName() const
{
    return "ping";
}

vector<uint8_t> CPingCommand::execute(const CommandContext& context)
{
    /* Ping doesn't require database connection */
    return executeWithoutDatabase(context);
}

bool CPingCommand::requiresDatabase() const
{
    return false; /* Ping should always work */
}

vector<uint8_t> CPingCommand::executeWithDatabase(const CommandContext& context)
{
    /* Same as without database for ping */
    return executeWithoutDatabase(context);
}

vector<uint8_t>
CPingCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple ping response */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 1.0);
    bson.endDocument();

    return bson.getDocument();
}

} // namespace FauxDB
