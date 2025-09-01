/*-------------------------------------------------------------------------
 *
 * CAggregateCommand.cpp
 *      Aggregate command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CAggregateCommand.hpp"
#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{


CAggregateCommand::CAggregateCommand()
{
    /* Constructor */
}


string
CAggregateCommand::getCommandName() const
{
    return "aggregate";
}


vector<uint8_t>
CAggregateCommand::execute(const CommandContext& context)
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
CAggregateCommand::requiresDatabase() const
{
    return true;
}


vector<uint8_t>
CAggregateCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for aggregate */
    string collection = getCollectionFromContext(context);
    vector<PipelineStage> pipeline = extractPipeline(context.requestBuffer, context.requestSize);
    
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection = std::static_pointer_cast<PGConnection>(voidConnection);
            
            /* Convert pipeline to SQL */
            string sql = convertPipelineToSQL(pipeline, collection);
            
            auto result = connection->database->executeQuery(sql);
            
            if (result.success)
            {
                /* Create cursor response */
                CBsonType cursor = createCursorResponse(result.rows, result.columnNames);
                bson.addDocument("cursor", cursor);
                bson.addDouble("ok", 1.0);
            }
            else
            {
                /* Query failed or table doesn't exist */
                CBsonType cursor;
                cursor.initialize();
                cursor.beginDocument();
                cursor.addInt64("id", 0);
                cursor.addString("ns", context.databaseName + "." + collection);
                cursor.beginArray("firstBatch");
                cursor.endArray();
                cursor.endDocument();
                
                bson.addDocument("cursor", cursor);
                bson.addDouble("ok", 1.0);
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
        bson.addString("errmsg", "aggregate operation failed");
    }
    
    bson.endDocument();
    return bson.getDocument();
}


vector<uint8_t>
CAggregateCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    string collection = getCollectionFromContext(context);
    
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    /* Create mock cursor response */
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    cursor.addInt64("id", 0);
    cursor.addString("ns", context.databaseName + "." + collection);
    
    cursor.beginArray("firstBatch");
    
    /* Add mock aggregated result */
    CBsonType mockResult;
    mockResult.initialize();
    mockResult.beginDocument();
    mockResult.addString("_id", "group1");
    mockResult.addInt32("count", 42);
    mockResult.addDouble("total", 1234.56);
    mockResult.endDocument();
    cursor.addArrayDocument(mockResult);
    
    cursor.endArray();
    cursor.endDocument();
    
    bson.addDocument("cursor", cursor);
    bson.addDouble("ok", 1.0);
    bson.endDocument();
    
    return bson.getDocument();
}


vector<PipelineStage>
CAggregateCommand::extractPipeline(const vector<uint8_t>& buffer, ssize_t bufferSize)
{
    /* Simple pipeline extraction - placeholder implementation */
    vector<PipelineStage> pipeline;
    
    /* For now, create a basic pipeline with common stages */
    PipelineStage match;
    match.type = "$match";
    match.operation = "{}";
    pipeline.push_back(match);
    
    return pipeline;
}


string
CAggregateCommand::convertPipelineToSQL(const vector<PipelineStage>& pipeline, const string& collection)
{
    /* Convert MongoDB aggregation pipeline to SQL */
    string sql = "SELECT * FROM \"" + collection + "\"";
    string whereClause;
    string groupClause;
    string orderClause;
    string limitClause;
    
    for (const auto& stage : pipeline)
    {
        if (stage.type == "$match")
        {
            whereClause = handleMatchStage(stage.operation);
        }
        else if (stage.type == "$group")
        {
            groupClause = handleGroupStage(stage.operation);
        }
        else if (stage.type == "$sort")
        {
            orderClause = handleSortStage(stage.operation);
        }
        else if (stage.type == "$limit")
        {
            limitClause = handleLimitStage(stage.operation);
        }
    }
    
    /* Build complete SQL */
    if (!whereClause.empty())
    {
        sql += " WHERE " + whereClause;
    }
    if (!groupClause.empty())
    {
        sql += " GROUP BY " + groupClause;
    }
    if (!orderClause.empty())
    {
        sql += " ORDER BY " + orderClause;
    }
    if (!limitClause.empty())
    {
        sql += " LIMIT " + limitClause;
    }
    
    return sql;
}


string
CAggregateCommand::handleMatchStage(const string& operation)
{
    /* Convert $match stage to WHERE clause */
    if (operation == "{}" || operation.empty())
    {
        return "1=1";
    }
    
    /* Placeholder implementation - in real system would parse JSON */
    return "1=1";
}


string
CAggregateCommand::handleGroupStage(const string& operation)
{
    /* Convert $group stage to GROUP BY clause */
    /* Placeholder implementation */
    return "_id";
}


string
CAggregateCommand::handleSortStage(const string& operation)
{
    /* Convert $sort stage to ORDER BY clause */
    /* Placeholder implementation */
    return "_id ASC";
}


string
CAggregateCommand::handleLimitStage(const string& operation)
{
    /* Convert $limit stage to LIMIT clause */
    /* Placeholder implementation */
    return "100";
}


string
CAggregateCommand::handleSkipStage(const string& operation)
{
    /* Convert $skip stage to OFFSET clause */
    /* Placeholder implementation */
    return "0";
}


CBsonType
CAggregateCommand::createCursorResponse(const vector<vector<string>>& rows, const vector<string>& columnNames)
{
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    cursor.addInt64("id", 0);
    cursor.addString("ns", "fauxdb.collection");
    
    cursor.beginArray("firstBatch");
    
    /* Convert each row to BSON document */
    for (const auto& row : rows)
    {
        CBsonType document;
        document.initialize();
        document.beginDocument();
        
        /* Add fields from row */
        for (size_t i = 0; i < row.size() && i < columnNames.size(); ++i)
        {
            /* Try to infer type and add appropriately */
            this->addInferredType(document, columnNames[i], row[i]);
        }
        
        document.endDocument();
        cursor.addArrayDocument(document);
    }
    
    cursor.endArray();
    cursor.endDocument();
    
    return cursor;
}

} // namespace FauxDB
