/*-------------------------------------------------------------------------
 *
 * CCreateCommand.cpp
 *      Create command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CCreateCommand.hpp"

#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{

CCreateCommand::CCreateCommand()
{
    /* Constructor */
}

string CCreateCommand::getCommandName() const
{
    return "create";
}

vector<uint8_t> CCreateCommand::execute(const CommandContext& context)
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

bool CCreateCommand::requiresDatabase() const
{
    return true;
}

vector<uint8_t>
CCreateCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for create collection */
    string collection = getCollectionFromContext(context);
    bool capped =
        extractCappedOption(context.requestBuffer, context.requestSize);
    int64_t size =
        extractSizeOption(context.requestBuffer, context.requestSize);
    int64_t max = extractMaxOption(context.requestBuffer, context.requestSize);

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

            string sql = buildCreateTableSQL(collection);

            auto result = connection->database->executeQuery(sql);

            if (result.success)
            {
                bson.addDouble("ok", 1.0);
            }
            else
            {

                bson.addDouble("ok", 0.0);
                bson.addString("errmsg", "collection already exists");
                bson.addInt32("code", 48);
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
        bson.addString("errmsg", "create operation failed");
    }

    bson.endDocument();
    return bson.getDocument();
}

vector<uint8_t>
CCreateCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 1.0);
    bson.endDocument();

    return bson.getDocument();
}

string CCreateCommand::buildCreateTableSQL(const string& collectionName)
{

    return "CREATE TABLE IF NOT EXISTS \"" + collectionName +
           "\" ("
           "_id VARCHAR(24) PRIMARY KEY, "
           "document JSONB NOT NULL, "
           "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
           "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
           ")";
}

bool CCreateCommand::extractCappedOption(const vector<uint8_t>& buffer,
                                         ssize_t bufferSize)
{
    /* Simple capped option extraction - placeholder implementation */
    return false;
}

int64_t CCreateCommand::extractSizeOption(const vector<uint8_t>& buffer,
                                          ssize_t bufferSize)
{
    /* Simple size option extraction - placeholder implementation */
    return 0;
}

int64_t CCreateCommand::extractMaxOption(const vector<uint8_t>& buffer,
                                         ssize_t bufferSize)
{
    /* Simple max option extraction - placeholder implementation */
    return 0;
}

} // namespace FauxDB
