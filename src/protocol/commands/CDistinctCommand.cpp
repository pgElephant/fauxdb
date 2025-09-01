/*-------------------------------------------------------------------------
 *
 * CDistinctCommand.cpp
 *      Distinct command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CDistinctCommand.hpp"

#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{

CDistinctCommand::CDistinctCommand()
{
    /* Constructor */
}

string CDistinctCommand::getCommandName() const
{
    return "distinct";
}

vector<uint8_t> CDistinctCommand::execute(const CommandContext& context)
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

bool CDistinctCommand::requiresDatabase() const
{
    return true;
}

vector<uint8_t>
CDistinctCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for distinct values */
    string collection = getCollectionFromContext(context);
    string fieldName =
        extractFieldName(context.requestBuffer, context.requestSize);

    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 1.0);

    /* Create values array with PostgreSQL data */
    bson.beginArray("values");

    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);
            /* For now, return some sample distinct values */
            bson.addArrayString("db_value1");
            bson.addArrayString("db_value2");
            bson.addArrayString("db_value3");

            context.connectionPooler->returnConnection(voidConnection);
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback to default values on error */
        bson.addArrayString("fallback_value1");
        bson.addArrayString("fallback_value2");
    }

    bson.endArray();
    bson.addString("stats", "");
    bson.endDocument();

    return bson.getDocument();
}

vector<uint8_t>
CDistinctCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 1.0);

    /* Create values array with sample data */
    bson.beginArray("values");
    bson.addArrayString("sample_value1");
    bson.addArrayString("sample_value2");
    bson.addArrayString("sample_value3");
    bson.endArray();

    bson.addString("stats", "");
    bson.endDocument();

    return bson.getDocument();
}

string CDistinctCommand::extractFieldName(const vector<uint8_t>& buffer,
                                          ssize_t bufferSize)
{
    /* Simple field extraction - placeholder implementation */
    /* Should parse BSON to find the "key" field value */
    return "defaultField";
}

} // namespace FauxDB
