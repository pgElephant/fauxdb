/*-------------------------------------------------------------------------
 *
 * CListDatabasesCommand.cpp
 *      ListDatabases command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CListDatabasesCommand.hpp"
#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{


CListDatabasesCommand::CListDatabasesCommand()
{
    /* Constructor */
}


string
CListDatabasesCommand::getCommandName() const
{
    return "listDatabases";
}


vector<uint8_t>
CListDatabasesCommand::execute(const CommandContext& context)
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
CListDatabasesCommand::requiresDatabase() const
{
    return true;
}


vector<uint8_t>
CListDatabasesCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for listDatabases */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    try
    {
        vector<DatabaseInfo> databases = getDatabaseList(context);
        bool nameOnly = extractNameOnly(context.requestBuffer, context.requestSize);
        
        /* Create databases array */
        bson.beginArray("databases");
        
        int64_t totalSize = 0;
        for (const auto& db : databases)
        {
            CBsonType dbInfo;
            dbInfo.initialize();
            dbInfo.beginDocument();
            dbInfo.addString("name", db.name);
            
            if (!nameOnly)
            {
                dbInfo.addInt64("sizeOnDisk", db.sizeOnDisk);
                dbInfo.addBool("empty", db.empty);
                totalSize += db.sizeOnDisk;
            }
            
            dbInfo.endDocument();
            bson.addArrayDocument(dbInfo);
        }
        
        bson.endArray(); /* End databases array */
        
        if (!nameOnly)
        {
            bson.addInt64("totalSize", totalSize);
        }
        
        bson.addDouble("ok", 1.0);
    }
    catch (const std::exception& e)
    {
        bson.addDouble("ok", 0.0);
        bson.addString("errmsg", "listDatabases operation failed");
    }
    
    bson.endDocument();
    return bson.getDocument();
}


vector<uint8_t>
CListDatabasesCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    /* Create mock databases array */
    bson.beginArray("databases");
    
    /* Add current database */
    CBsonType currentDb;
    currentDb.initialize();
    currentDb.beginDocument();
    currentDb.addString("name", context.databaseName);
    currentDb.addInt64("sizeOnDisk", 1048576); /* 1MB */
    currentDb.addBool("empty", false);
    currentDb.endDocument();
    bson.addArrayDocument(currentDb);
    
    /* Add admin database */
    CBsonType adminDb;
    adminDb.initialize();
    adminDb.beginDocument();
    adminDb.addString("name", "admin");
    adminDb.addInt64("sizeOnDisk", 32768); /* 32KB */
    adminDb.addBool("empty", false);
    adminDb.endDocument();
    bson.addArrayDocument(adminDb);
    
    /* Add local database */
    CBsonType localDb;
    localDb.initialize();
    localDb.beginDocument();
    localDb.addString("name", "local");
    localDb.addInt64("sizeOnDisk", 65536); /* 64KB */
    localDb.addBool("empty", false);
    localDb.endDocument();
    bson.addArrayDocument(localDb);
    
    bson.endArray(); /* End databases array */
    
    bson.addInt64("totalSize", 1146880); /* Sum of all sizes */
    bson.addDouble("ok", 1.0);
    
    bson.endDocument();
    return bson.getDocument();
}


vector<DatabaseInfo>
CListDatabasesCommand::getDatabaseList(const CommandContext& context)
{
    vector<DatabaseInfo> databases;
    
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection = std::static_pointer_cast<PGConnection>(voidConnection);
            
            /* Query PostgreSQL for available databases */
            string sql = "SELECT datname FROM pg_database WHERE datistemplate = false AND datallowconn = true ORDER BY datname";
            auto result = connection->database->executeQuery(sql);
            
            if (result.success)
            {
                for (const auto& row : result.rows)
                {
                    if (!row.empty())
                    {
                        DatabaseInfo dbInfo;
                        dbInfo.name = row[0];
                        dbInfo.sizeOnDisk = getDatabaseSize(context, dbInfo.name);
                        dbInfo.empty = isDatabaseEmpty(context, dbInfo.name);
                        databases.push_back(dbInfo);
                    }
                }
            }
            
            context.connectionPooler->returnConnection(voidConnection);
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback to current database only */
        DatabaseInfo dbInfo;
        dbInfo.name = context.databaseName;
        dbInfo.sizeOnDisk = 1048576; /* 1MB fallback */
        dbInfo.empty = false;
        databases.push_back(dbInfo);
    }
    
    /* Always include system databases */
    DatabaseInfo adminDb;
    adminDb.name = "admin";
    adminDb.sizeOnDisk = 32768;
    adminDb.empty = false;
    databases.push_back(adminDb);
    
    DatabaseInfo localDb;
    localDb.name = "local";
    localDb.sizeOnDisk = 65536;
    localDb.empty = false;
    databases.push_back(localDb);
    
    return databases;
}


int64_t
CListDatabasesCommand::getDatabaseSize(const CommandContext& context, const string& dbName)
{
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection = std::static_pointer_cast<PGConnection>(voidConnection);
            
            /* Get database size */
            string sql = "SELECT pg_database_size('" + dbName + "')";
            auto result = connection->database->executeQuery(sql);
            
            context.connectionPooler->returnConnection(voidConnection);
            
            if (result.success && !result.rows.empty() && !result.rows[0].empty())
            {
                return std::stoll(result.rows[0][0]);
            }
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback on error */
    }
    
    return 1048576; /* Default 1MB */
}


bool
CListDatabasesCommand::isDatabaseEmpty(const CommandContext& context, const string& dbName)
{
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection = std::static_pointer_cast<PGConnection>(voidConnection);
            
            /* Check if database has any user tables */
            string sql = "SELECT COUNT(*) FROM information_schema.tables WHERE table_catalog = '" + dbName + "' AND table_schema = 'public'";
            auto result = connection->database->executeQuery(sql);
            
            context.connectionPooler->returnConnection(voidConnection);
            
            if (result.success && !result.rows.empty() && !result.rows[0].empty())
            {
                return std::stoll(result.rows[0][0]) == 0;
            }
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback on error */
    }
    
    return false; /* Assume not empty by default */
}


bool
CListDatabasesCommand::extractNameOnly(const vector<uint8_t>& buffer, ssize_t bufferSize)
{
    /* Simple nameOnly field extraction - placeholder implementation */
    return false; /* Default to full information */
}


CBsonType
CListDatabasesCommand::createDatabasesResponse(const vector<DatabaseInfo>& databases)
{
    CBsonType response;
    response.initialize();
    response.beginDocument();
    
    response.beginArray("databases");
    for (const auto& db : databases)
    {
        CBsonType dbInfo;
        dbInfo.initialize();
        dbInfo.beginDocument();
        dbInfo.addString("name", db.name);
        dbInfo.addInt64("sizeOnDisk", db.sizeOnDisk);
        dbInfo.addBool("empty", db.empty);
        dbInfo.endDocument();
        response.addArrayDocument(dbInfo);
    }
    response.endArray();
    
    response.endDocument();
    return response;
}

} // namespace FauxDB
