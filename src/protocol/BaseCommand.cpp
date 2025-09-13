/*-------------------------------------------------------------------------
 *
 * BaseCommand.cpp
 *      Base command class for MongoDB command implementations.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

/* Base MongoDB Command Implementation */
#include "protocol/BaseCommand.hpp"

#include "../CServerConfig.hpp"
#include "CLogger.hpp"
#include "database/CPGConnectionPooler.hpp"

#include <sstream>

namespace FauxDB
{

BaseCommand::BaseCommand() : logger_(std::make_shared<CLogger>(CServerConfig{}))
{
}

CBsonType BaseCommand::createBaseResponse(bool success)
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", success ? 1.0 : 0.0);
    return bson;
}

shared_ptr<CPostgresDatabase>
BaseCommand::getConnection(shared_ptr<CPGConnectionPooler> connectionPooler)
{
    if (!connectionPooler)
    {
        return nullptr;
    }

    try
    {
        auto pgConnection = connectionPooler->getPostgresConnection();
        if (pgConnection && pgConnection->database)
        {
            return pgConnection->database;
        }
    }
    catch (const std::exception& e)
    {
        /* Connection failed */
    }

    return nullptr;
}

void BaseCommand::releaseConnection(
    shared_ptr<CPGConnectionPooler> connectionPooler,
    shared_ptr<void> voidConnection)
{
    if (connectionPooler && voidConnection)
    {
        connectionPooler->releaseConnection(voidConnection);
    }
}

CBsonType BaseCommand::rowToBsonDocument(const vector<string>& row,
                                         const vector<string>& columnNames)
{
    CBsonType document;
    document.initialize();
    document.beginDocument();
    bool hasId = false;

    for (size_t j = 0; j < columnNames.size() && j < row.size(); ++j)
    {
        const string& colName = columnNames[j];
        const string& value = row[j];

        if (colName == "_id" || colName == "id")
        {
            document.addString("_id", value);
            hasId = true;
        }
        else
        {
            addInferredType(document, colName, value);
        }
    }

    /* Add generated _id if not present */
    if (!hasId)
    {
        document.addString("_id", "pg_generated_id");
    }

    document.endDocument();
    return document;
}

void BaseCommand::addInferredType(CBsonType& bson, const string& fieldName,
                                  const string& value)
{
    /* Simple type inference */
    if (value == "true" || value == "false")
    {
        bson.addBool(fieldName, value == "true");
    }
    else if (value.find('.') != string::npos)
    {
        try
        {
            bson.addDouble(fieldName, std::stod(value));
        }
        catch (...)
        {
            bson.addString(fieldName, value);
        }
    }
    else
    {
        try
        {
            bson.addInt32(fieldName, std::stoi(value));
        }
        catch (...)
        {
            bson.addString(fieldName, value);
        }
    }
}

} /* namespace FauxDB */
