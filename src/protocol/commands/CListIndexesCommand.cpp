/*-------------------------------------------------------------------------
 *
 * CListIndexesCommand.cpp
 *      ListIndexes command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CListIndexesCommand.hpp"
#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{


CListIndexesCommand::CListIndexesCommand()
{
    /* Constructor */
}


string
CListIndexesCommand::getCommandName() const
{
    return "listIndexes";
}


vector<uint8_t>
CListIndexesCommand::execute(const CommandContext& context)
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
CListIndexesCommand::requiresDatabase() const
{
    return true;
}


vector<uint8_t>
CListIndexesCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for listIndexes */
    string collection = getCollectionFromContext(context);
    
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    try
    {
        vector<IndexInfo> indexes = getCollectionIndexes(context, collection);
        string ns = context.databaseName + "." + collection;
        
        /* Create cursor response */
        CBsonType cursor = createCursorResponse(indexes, ns);
        bson.addDocument("cursor", cursor);
        bson.addDouble("ok", 1.0);
    }
    catch (const std::exception& e)
    {
        bson.addDouble("ok", 0.0);
        bson.addString("errmsg", "listIndexes operation failed");
        bson.addInt32("code", 26); /* NamespaceNotFound */
    }
    
    bson.endDocument();
    return bson.getDocument();
}


vector<uint8_t>
CListIndexesCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    string collection = getCollectionFromContext(context);
    string ns = context.databaseName + "." + collection;
    
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    /* Create mock indexes */
    vector<IndexInfo> mockIndexes;
    
    /* Default _id index */
    IndexInfo idIndex;
    idIndex.name = "_id_";
    idIndex.keyPattern = "{\"_id\": 1}";
    idIndex.version = 2;
    idIndex.unique = true;
    idIndex.sparse = false;
    idIndex.ns = ns;
    mockIndexes.push_back(idIndex);
    
    /* Example secondary index */
    IndexInfo nameIndex;
    nameIndex.name = "name_1";
    nameIndex.keyPattern = "{\"name\": 1}";
    nameIndex.version = 2;
    nameIndex.unique = false;
    nameIndex.sparse = false;
    nameIndex.ns = ns;
    mockIndexes.push_back(nameIndex);
    
    /* Create cursor response */
    CBsonType cursor = createCursorResponse(mockIndexes, ns);
    bson.addDocument("cursor", cursor);
    bson.addDouble("ok", 1.0);
    
    bson.endDocument();
    return bson.getDocument();
}


vector<IndexInfo>
CListIndexesCommand::getCollectionIndexes(const CommandContext& context, const string& collection)
{
    vector<IndexInfo> indexes;
    string ns = context.databaseName + "." + collection;
    
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection = std::static_pointer_cast<PGConnection>(voidConnection);
            
            /* Query PostgreSQL for indexes on the table */
            string sql = "SELECT indexname, indexdef FROM pg_indexes WHERE tablename = '" + collection + "'";
            auto result = connection->database->executeQuery(sql);
            
            if (result.success)
            {
                for (const auto& row : result.rows)
                {
                    if (row.size() >= 2)
                    {
                        IndexInfo info;
                        info.name = row[0];
                        info.keyPattern = "{\"field\": 1}"; /* Simplified - would parse indexdef */
                        info.version = 2;
                        info.unique = row[1].find("UNIQUE") != string::npos;
                        info.sparse = false;
                        info.ns = ns;
                        indexes.push_back(info);
                    }
                }
            }
            
            context.connectionPooler->returnConnection(voidConnection);
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback to default indexes */
    }
    
    /* Always include default _id index if not found */
    bool hasIdIndex = false;
    for (const auto& idx : indexes)
    {
        if (idx.name.find("_id") != string::npos)
        {
            hasIdIndex = true;
            break;
        }
    }
    
    if (!hasIdIndex)
    {
        IndexInfo idIndex;
        idIndex.name = "_id_";
        idIndex.keyPattern = "{\"_id\": 1}";
        idIndex.version = 2;
        idIndex.unique = true;
        idIndex.sparse = false;
        idIndex.ns = ns;
        indexes.insert(indexes.begin(), idIndex);
    }
    
    return indexes;
}


CBsonType
CListIndexesCommand::createIndexInfoDocument(const IndexInfo& info)
{
    CBsonType doc;
    doc.initialize();
    doc.beginDocument();
    
    doc.addInt32("v", info.version);
    
    /* Parse and add key pattern */
    CBsonType key;
    key.initialize();
    key.beginDocument();
    
    /* Simplified key pattern parsing */
    if (info.keyPattern.find("_id") != string::npos)
    {
        key.addInt32("_id", 1);
    }
    else if (info.keyPattern.find("name") != string::npos)
    {
        key.addInt32("name", 1);
    }
    else
    {
        key.addInt32("field", 1);
    }
    
    key.endDocument();
    doc.addDocument("key", key);
    
    doc.addString("name", info.name);
    doc.addString("ns", info.ns);
    
    if (info.unique)
    {
        doc.addBool("unique", true);
    }
    
    if (info.sparse)
    {
        doc.addBool("sparse", true);
    }
    
    doc.endDocument();
    return doc;
}


CBsonType
CListIndexesCommand::createCursorResponse(const vector<IndexInfo>& indexes, const string& ns)
{
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    
    cursor.addInt64("id", 0); /* No cursor needed for indexes */
    cursor.addString("ns", ns + ".$cmd.listIndexes");
    
    /* Create firstBatch array */
    cursor.beginArray("firstBatch");
    
    for (const auto& info : indexes)
    {
        CBsonType indexDoc = createIndexInfoDocument(info);
        cursor.addArrayDocument(indexDoc);
    }
    
    cursor.endArray();
    cursor.endDocument();
    
    return cursor;
}

} // namespace FauxDB
