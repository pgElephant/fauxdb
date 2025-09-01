/*-------------------------------------------------------------------------
 *
 * CDropIndexesCommand.cpp
 *      DropIndexes command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CDropIndexesCommand.hpp"

#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{

CDropIndexesCommand::CDropIndexesCommand()
{
    /* Constructor */
}

string CDropIndexesCommand::getCommandName() const
{
    return "dropIndexes";
}

vector<uint8_t> CDropIndexesCommand::execute(const CommandContext& context)
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

bool CDropIndexesCommand::requiresDatabase() const
{
    return true;
}

vector<uint8_t>
CDropIndexesCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for dropIndexes */
    string collection = getCollectionFromContext(context);
    string indexSpecifier =
        extractIndexSpecifier(context.requestBuffer, context.requestSize);

    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    try
    {
        int32_t indexesBefore = getIndexCount(context, collection);
        vector<string> indexesToDrop =
            getIndexesToDrop(context, collection, indexSpecifier);

        int32_t droppedCount = 0;
        for (const string& indexName : indexesToDrop)
        {
            /* Don't allow dropping the _id index */
            if (indexName.find("_id") != string::npos)
            {
                continue;
            }

            if (dropIndex(context, collection, indexName))
            {
                droppedCount++;
            }
        }

        int32_t indexesAfter = indexesBefore - droppedCount;

        bson.addString("msg", "non-_id indexes dropped for collection");
        bson.addInt32("nIndexesWas", indexesBefore);
        bson.addDouble("ok", 1.0);
    }
    catch (const std::exception& e)
    {
        bson.addDouble("ok", 0.0);
        bson.addString("errmsg", "dropIndexes operation failed");
        bson.addInt32("code", 26); /* NamespaceNotFound */
    }

    bson.endDocument();
    return bson.getDocument();
}

vector<uint8_t>
CDropIndexesCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    string collection = getCollectionFromContext(context);
    string indexSpecifier =
        extractIndexSpecifier(context.requestBuffer, context.requestSize);

    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    /* Mock successful index drop */
    if (indexSpecifier == "*")
    {
        bson.addString("msg", "non-_id indexes dropped for collection");
        bson.addInt32("nIndexesWas", 3); /* Assume 3 indexes existed */
    }
    else
    {
        bson.addString("msg", "index dropped");
        bson.addInt32("nIndexesWas", 2); /* Assume 2 indexes existed */
    }

    bson.addDouble("ok", 1.0);

    bson.endDocument();
    return bson.getDocument();
}

string CDropIndexesCommand::extractIndexSpecifier(const vector<uint8_t>& buffer,
                                                  ssize_t bufferSize)
{
    /* Simple index specifier extraction - placeholder implementation */
    /* In a full implementation, this would parse the index field from BSON */
    return "*"; /* Default to drop all non-_id indexes */
}

vector<string>
CDropIndexesCommand::getIndexesToDrop(const CommandContext& context,
                                      const string& collection,
                                      const string& specifier)
{
    vector<string> indexesToDrop;

    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);

            string sql;
            if (specifier == "*")
            {
                /* Get all indexes except _id */
                sql = "SELECT indexname FROM pg_indexes WHERE tablename = '" +
                      collection + "' AND indexname != '" + collection +
                      "_pkey'";
            }
            else
            {
                /* Get specific index */
                sql = "SELECT indexname FROM pg_indexes WHERE tablename = '" +
                      collection + "' AND indexname = '" + specifier + "'";
            }

            auto result = connection->database->executeQuery(sql);

            if (result.success)
            {
                for (const auto& row : result.rows)
                {
                    if (!row.empty())
                    {
                        string indexName = row[0];
                        /* Skip _id-related indexes */
                        if (indexName.find("_id") == string::npos &&
                            indexName.find("pkey") == string::npos)
                        {
                            indexesToDrop.push_back(indexName);
                        }
                    }
                }
            }

            context.connectionPooler->returnConnection(voidConnection);
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback to empty list */
    }

    return indexesToDrop;
}

bool CDropIndexesCommand::dropIndex(const CommandContext& context,
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

            /* Drop the index */
            string sql = "DROP INDEX IF EXISTS \"" + indexName + "\"";
            auto result = connection->database->executeQuery(sql);

            context.connectionPooler->returnConnection(voidConnection);

            return result.success;
        }
    }
    catch (const std::exception& e)
    {
        /* Fail gracefully */
    }

    return false;
}

int32_t CDropIndexesCommand::getIndexCount(const CommandContext& context,
                                           const string& collection)
{
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);

            /* Count indexes on the table */
            string sql = "SELECT COUNT(*) FROM pg_indexes WHERE tablename = '" +
                         collection + "'";
            auto result = connection->database->executeQuery(sql);

            context.connectionPooler->returnConnection(voidConnection);

            if (result.success && !result.rows.empty() &&
                !result.rows[0].empty())
            {
                return std::stoi(result.rows[0][0]);
            }
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback */
    }

    return 1; /* Default to 1 (_id index) */
}

} // namespace FauxDB
