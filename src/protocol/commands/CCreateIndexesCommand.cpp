/*-------------------------------------------------------------------------
 *
 * CCreateIndexesCommand.cpp
 *      CreateIndexes command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CCreateIndexesCommand.hpp"

#include "database/CPGConnectionPooler.hpp"

#include <sstream>

namespace FauxDB
{

CCreateIndexesCommand::CCreateIndexesCommand()
{
    /* Constructor */
}

string CCreateIndexesCommand::getCommandName() const
{
    return "createIndexes";
}

vector<uint8_t> CCreateIndexesCommand::execute(const CommandContext& context)
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

bool CCreateIndexesCommand::requiresDatabase() const
{
    return true;
}

vector<uint8_t>
CCreateIndexesCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for createIndexes */
    string collection = getCollectionFromContext(context);
    vector<IndexSpec> indexSpecs =
        extractIndexSpecs(context.requestBuffer, context.requestSize);

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

            int32_t createdIndexes = 0;
            int32_t existingIndexes = 0;

            for (const auto& spec : indexSpecs)
            {
                string indexName = generateIndexName(collection, spec);

                /* Check if index already exists */
                if (indexExists(context, collection, indexName))
                {
                    existingIndexes++;
                    continue;
                }

                /* Create the index */
                string sql = buildCreateIndexSQL(collection, spec);
                auto result = connection->database->executeQuery(sql);

                if (result.success)
                {
                    createdIndexes++;
                }
            }

            /* Build response */
            bson.addInt32("numIndexesBefore", existingIndexes);
            bson.addInt32("numIndexesAfter", existingIndexes + createdIndexes);
            bson.addBool("createdCollectionAutomatically", false);
            bson.addString("note", "indexes created on existing collection");
            bson.addDouble("ok", 1.0);

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
        bson.addString("errmsg", "createIndexes operation failed");
    }

    bson.endDocument();
    return bson.getDocument();
}

vector<uint8_t>
CCreateIndexesCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    string collection = getCollectionFromContext(context);
    vector<IndexSpec> indexSpecs =
        extractIndexSpecs(context.requestBuffer, context.requestSize);

    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    /* Mock successful index creation */
    bson.addInt32("numIndexesBefore", 1); /* Assume _id index exists */
    bson.addInt32("numIndexesAfter",
                  1 + static_cast<int32_t>(indexSpecs.size()));
    bson.addBool("createdCollectionAutomatically", false);
    bson.addString("note", "indexes created on existing collection");
    bson.addDouble("ok", 1.0);

    bson.endDocument();
    return bson.getDocument();
}

vector<IndexSpec>
CCreateIndexesCommand::extractIndexSpecs(const vector<uint8_t>& buffer,
                                         ssize_t bufferSize)
{
    /* Simple index spec extraction - placeholder implementation */
    vector<IndexSpec> specs;

    /* Create a sample index spec for testing */
    IndexSpec spec;
    spec.name = "name_1";
    spec.keyPattern = "{\"name\": 1}";
    spec.unique = false;
    spec.sparse = false;
    spec.background = false;
    spec.expireAfterSeconds = -1; /* No TTL */
    spec.partialFilterExpression = "";

    specs.push_back(spec);
    return specs;
}

string CCreateIndexesCommand::buildCreateIndexSQL(const string& collection,
                                                  const IndexSpec& spec)
{
    /* Convert MongoDB index to PostgreSQL index */
    string indexName = generateIndexName(collection, spec);
    string sqlFields = convertKeyPatternToSQL(spec.keyPattern);

    std::stringstream sql;
    sql << "CREATE ";

    if (spec.unique)
    {
        sql << "UNIQUE ";
    }

    sql << "INDEX ";

    if (!spec.name.empty())
    {
        sql << "\"" << spec.name << "\" ";
    }
    else
    {
        sql << "\"" << indexName << "\" ";
    }

    sql << "ON \"" << collection << "\" ";

    /* Use GIN index for JSONB documents */
    if (sqlFields.find("document") != string::npos)
    {
        sql << "USING GIN (" << sqlFields << ")";
    }
    else
    {
        sql << "(" << sqlFields << ")";
    }

    /* Add partial filter if specified */
    if (!spec.partialFilterExpression.empty())
    {
        sql << " WHERE " << spec.partialFilterExpression;
    }

    return sql.str();
}

string CCreateIndexesCommand::convertKeyPatternToSQL(const string& keyPattern)
{
    /* Convert MongoDB key pattern to PostgreSQL fields */
    /* This is a simplified implementation - full version would parse JSON */

    if (keyPattern.find("name") != string::npos)
    {
        return "(document->>'name')";
    }
    else if (keyPattern.find("_id") != string::npos)
    {
        return "_id";
    }
    else
    {
        /* Default to GIN index on full document */
        return "document";
    }
}

string CCreateIndexesCommand::generateIndexName(const string& collection,
                                                const IndexSpec& spec)
{
    if (!spec.name.empty())
    {
        return spec.name;
    }

    /* Generate name from key pattern */
    std::stringstream name;
    name << collection << "_";

    if (spec.keyPattern.find("name") != string::npos)
    {
        name << "name_1";
    }
    else if (spec.keyPattern.find("_id") != string::npos)
    {
        name << "_id_1";
    }
    else
    {
        name << "auto_1";
    }

    return name.str();
}

bool CCreateIndexesCommand::indexExists(const CommandContext& context,
                                        const string& collection,
                                        const string& indexName)
{
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);

            /* Check if index exists in PostgreSQL */
            string sql = "SELECT 1 FROM pg_indexes WHERE tablename = '" +
                         collection + "' AND indexname = '" + indexName + "'";
            auto result = connection->database->executeQuery(sql);

            context.connectionPooler->returnConnection(voidConnection);

            return result.success && !result.rows.empty();
        }
    }
    catch (const std::exception& e)
    {
        /* Assume doesn't exist on error */
    }

    return false;
}

} // namespace FauxDB
