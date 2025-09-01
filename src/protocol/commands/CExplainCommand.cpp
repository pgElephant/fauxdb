/*-------------------------------------------------------------------------
 *
 * CExplainCommand.cpp
 *      Explain command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CExplainCommand.hpp"

#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{

CExplainCommand::CExplainCommand()
{
    /* Constructor */
}

string CExplainCommand::getCommandName() const
{
    return "explain";
}

vector<uint8_t> CExplainCommand::execute(const CommandContext& context)
{
    if (context.connectionPooler && requiresDatabase())
    {
        return executeWithDatabase(context);
    }
    else
    {
        return executeWithoutDatabase(context);
    }
}

bool CExplainCommand::requiresDatabase() const
{
    return false; /* Can work without database for basic explain */
}

vector<uint8_t>
CExplainCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for explain */
    string collection = getCollectionFromContext(context);
    string explainedCommand =
        extractExplainedCommand(context.requestBuffer, context.requestSize);
    string verbosity =
        extractVerbosity(context.requestBuffer, context.requestSize);

    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);

            /* Create explain result */
            bson.addDouble("ok", 1.0);

            /* Add query planner info */
            CBsonType queryPlanner =
                createQueryPlanner(collection, explainedCommand);
            bson.addDocument("queryPlanner", queryPlanner);

            /* Add execution stats if requested */
            if (verbosity == "executionStats" ||
                verbosity == "allPlansExecution")
            {
                CBsonType executionStats = createExecutionStats();
                bson.addDocument("executionStats", executionStats);
            }

            context.connectionPooler->returnConnection(voidConnection);
        }
        else
        {
            bson.addDouble("ok", 0.0);
            bson.addString("errmsg", "database connection failed");
        }
    }
    catch (const std::exception& e)
    {
        bson.addDouble("ok", 0.0);
        bson.addString("errmsg", "explain operation failed");
    }

    bson.endDocument();
    return bson.getDocument();
}

vector<uint8_t>
CExplainCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    string collection = getCollectionFromContext(context);
    string explainedCommand =
        extractExplainedCommand(context.requestBuffer, context.requestSize);

    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 1.0);

    /* Add basic query planner info */
    CBsonType queryPlanner = createQueryPlanner(collection, explainedCommand);
    bson.addDocument("queryPlanner", queryPlanner);

    bson.endDocument();
    return bson.getDocument();
}

string CExplainCommand::extractExplainedCommand(const vector<uint8_t>& buffer,
                                                ssize_t bufferSize)
{
    /* Simple command extraction - placeholder implementation */
    return "find";
}

string CExplainCommand::extractVerbosity(const vector<uint8_t>& buffer,
                                         ssize_t bufferSize)
{
    /* Simple verbosity extraction - placeholder implementation */
    return "queryPlanner";
}

CBsonType CExplainCommand::createExecutionStats()
{
    CBsonType stats;
    stats.initialize();
    stats.beginDocument();
    stats.addInt32("totalDocsExamined", 100);
    stats.addInt32("totalDocsReturned", 25);
    stats.addInt32("executionTimeMillis", 15);
    stats.addString("stage", "COLLSCAN");
    stats.addBool("isEOF", true);
    stats.endDocument();
    return stats;
}

CBsonType CExplainCommand::createQueryPlanner(const string& collection,
                                              const string& command)
{
    CBsonType planner;
    planner.initialize();
    planner.beginDocument();
    planner.addInt32("plannerVersion", 1);
    planner.addString("namespace", collection);
    planner.addBool("indexFilterSet", false);
    planner.addBool("parsedQuery", true);

    /* Add winning plan */
    CBsonType winningPlan;
    winningPlan.initialize();
    winningPlan.beginDocument();
    winningPlan.addString("stage", "COLLSCAN");
    winningPlan.addString("direction", "forward");
    winningPlan.endDocument();
    planner.addDocument("winningPlan", winningPlan);

    planner.endDocument();
    return planner;
}

} // namespace FauxDB
