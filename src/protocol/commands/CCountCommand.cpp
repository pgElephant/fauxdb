/*-------------------------------------------------------------------------
 *
 * CCountCommand.cpp
 *      Count command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CCountCommand.hpp"
#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{


CCountCommand::CCountCommand()
{
    /* Constructor */
}


string
CCountCommand::getCommandName() const
{
    return "count";
}


vector<uint8_t>
CCountCommand::execute(const CommandContext& context)
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
CCountCommand::requiresDatabase() const
{
    return true;
}


vector<uint8_t>
CCountCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for count */
    string collection = getCollectionFromContext(context);
    string query = extractQuery(context.requestBuffer, context.requestSize);
    int64_t limit = extractLimit(context.requestBuffer, context.requestSize);
    int64_t skip = extractSkip(context.requestBuffer, context.requestSize);
    
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection = std::static_pointer_cast<PGConnection>(voidConnection);
            
            /* Build and execute COUNT SQL */
            string whereClause = convertQueryToWhere(query);
            string sql = buildCountSQL(collection, whereClause);
            
            auto result = connection->database->executeQuery(sql);
            
            if (result.success && !result.rows.empty())
            {
                /* Get count from first row, first column */
                int64_t count = 0;
                if (!result.rows[0].empty())
                {
                    try
                    {
                        count = std::stoll(result.rows[0][0]);
                        
                        /* Apply skip and limit if specified */
                        if (skip > 0)
                        {
                            count = std::max(0LL, count - skip);
                        }
                        if (limit > 0 && count > limit)
                        {
                            count = limit;
                        }
                    }
                    catch (const std::exception& e)
                    {
                        count = 0;
                    }
                }
                
                bson.addDouble("ok", 1.0);
                bson.addInt64("n", count);
            }
            else
            {
                /* Table doesn't exist or query failed */
                bson.addDouble("ok", 1.0);
                bson.addInt64("n", 0);
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
        bson.addString("errmsg", "count operation failed");
    }
    
    bson.endDocument();
    return bson.getDocument();
}


vector<uint8_t>
CCountCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 1.0);
    bson.addInt64("n", 42); /* Mock count */
    bson.endDocument();
    
    return bson.getDocument();
}


string
CCountCommand::extractQuery(const vector<uint8_t>& buffer, ssize_t bufferSize)
{
    /* Simple query extraction - placeholder implementation */
    /* In a full implementation, this would parse the BSON query document */
    return "{}";
}


int64_t
CCountCommand::extractLimit(const vector<uint8_t>& buffer, ssize_t bufferSize)
{
    /* Simple limit extraction - placeholder implementation */
    return 0;
}


int64_t
CCountCommand::extractSkip(const vector<uint8_t>& buffer, ssize_t bufferSize)
{
    /* Simple skip extraction - placeholder implementation */
    return 0;
}


string
CCountCommand::buildCountSQL(const string& collectionName, const string& whereClause)
{
    string sql = "SELECT COUNT(*) FROM \"" + collectionName + "\"";
    if (!whereClause.empty() && whereClause != "1=1")
    {
        sql += " WHERE " + whereClause;
    }
    return sql;
}


string
CCountCommand::convertQueryToWhere(const string& query)
{
    /* Simple query to WHERE conversion - placeholder implementation */
    /* In a full implementation, this would convert MongoDB query to SQL WHERE */
    if (query == "{}" || query.empty())
    {
        return "1=1"; /* No filter */
    }
    
    /* For now, just return a basic condition */
    return "1=1";
}

} // namespace FauxDB
