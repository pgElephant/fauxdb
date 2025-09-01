/*-------------------------------------------------------------------------
 *
 * CDropCommand.cpp
 *      Drop command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CDropCommand.hpp"
#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{


CDropCommand::CDropCommand()
{
    /* Constructor */
}


string
CDropCommand::getCommandName() const
{
    return "drop";
}


vector<uint8_t>
CDropCommand::execute(const CommandContext& context)
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


bool
CDropCommand::requiresDatabase() const
{
    return true;
}


vector<uint8_t>
CDropCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for drop collection */
    string collection = getCollectionFromContext(context);
    
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection = std::static_pointer_cast<PGConnection>(voidConnection);
            
            /* Build and execute DROP TABLE SQL */
            string sql = buildDropTableSQL(collection);
            
            /* Execute the drop command */
            auto result = connection->database->executeQuery(sql);
            
            if (result.success)
            {
                bson.addDouble("ok", 1.0);
                bson.addInt32("nIndexesWas", 1); /* Simulate index count */
                bson.addString("ns", context.databaseName + "." + collection);
            }
            else
            {
                bson.addDouble("ok", 0.0);
                bson.addString("errmsg", "collection not found");
                bson.addInt32("code", 26); /* NamespaceNotFound */
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
        bson.addString("errmsg", "drop operation failed");
    }
    
    bson.endDocument();
    return bson.getDocument();
}


vector<uint8_t>
CDropCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 1.0);
    bson.addInt32("nIndexesWas", 1);
    bson.addString("ns", context.databaseName + "." + getCollectionFromContext(context));
    bson.endDocument();
    
    return bson.getDocument();
}


string
CDropCommand::buildDropTableSQL(const string& collectionName)
{
    return "DROP TABLE IF EXISTS \"" + collectionName + "\"";
}

} // namespace FauxDB
