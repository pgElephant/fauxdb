/*-------------------------------------------------------------------------
 *
 * CDbStatsCommand.cpp
 *      DbStats command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CDbStatsCommand.hpp"

#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{

CDbStatsCommand::CDbStatsCommand()
{
    /* Constructor */
}

string CDbStatsCommand::getCommandName() const
{
    return "dbStats";
}

vector<uint8_t> CDbStatsCommand::execute(const CommandContext& context)
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

bool CDbStatsCommand::requiresDatabase() const
{
    return true;
}

vector<uint8_t>
CDbStatsCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for dbStats */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    try
    {
        DatabaseStats stats = collectDatabaseStats(context);
        CBsonType statsResponse = createStatsResponse(stats);

        /* Copy all fields from stats response */
        bson.addString("db", stats.db);
        bson.addInt64("collections", stats.collections);
        bson.addInt64("views",
                      0); /* PostgreSQL doesn't have MongoDB-style views */
        bson.addInt64("objects", stats.objects);
        bson.addDouble("avgObjSize", stats.avgObjSize);
        bson.addInt64("dataSize", stats.dataSize);
        bson.addInt64("storageSize", stats.storageSize);
        bson.addInt64("indexes", stats.indexes);
        bson.addInt64("indexSize", stats.indexSize);
        bson.addInt64("totalSize", stats.totalSize);
        bson.addDouble("scaleFactor", stats.scaleFactor);
        bson.addDouble("ok", 1.0);
    }
    catch (const std::exception& e)
    {
        bson.addDouble("ok", 0.0);
        bson.addString("errmsg", "dbStats operation failed");
    }

    bson.endDocument();
    return bson.getDocument();
}

vector<uint8_t>
CDbStatsCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    /* Mock database statistics */
    bson.addString("db", context.databaseName);
    bson.addInt64("collections", 5);
    bson.addInt64("views", 0);
    bson.addInt64("objects", 1000);
    bson.addDouble("avgObjSize", 512.0);
    bson.addInt64("dataSize", 512000);
    bson.addInt64("storageSize", 1024000);
    bson.addInt64("indexes", 10);
    bson.addInt64("indexSize", 204800);
    bson.addInt64("totalSize", 1228800);
    bson.addDouble("scaleFactor", 1.0);
    bson.addDouble("ok", 1.0);

    bson.endDocument();
    return bson.getDocument();
}

DatabaseStats
CDbStatsCommand::collectDatabaseStats(const CommandContext& context)
{
    DatabaseStats stats;
    stats.db = context.databaseName;
    stats.scaleFactor =
        extractScale(context.requestBuffer, context.requestSize);

    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);

            stats.collections = getTableCount(context);
            stats.objects = getTotalRows(context);
            stats.storageSize = getStorageSize(context);

            /* Calculate derived statistics */
            stats.views = 0; /* PostgreSQL views are not MongoDB views */
            stats.dataSize =
                stats.storageSize * 0.8; /* Estimate 80% data, 20% overhead */
            stats.avgObjSize = stats.objects > 0
                                   ? (double)stats.dataSize / stats.objects
                                   : 0.0;
            stats.indexes =
                stats.collections * 2; /* Estimate 2 indexes per table */
            stats.indexSize =
                stats.dataSize * 0.2; /* Estimate 20% for indexes */
            stats.totalSize = stats.dataSize + stats.indexSize;

            /* Apply scale factor */
            if (stats.scaleFactor != 1.0)
            {
                stats.dataSize = (int64_t)(stats.dataSize / stats.scaleFactor);
                stats.storageSize =
                    (int64_t)(stats.storageSize / stats.scaleFactor);
                stats.indexSize =
                    (int64_t)(stats.indexSize / stats.scaleFactor);
                stats.totalSize =
                    (int64_t)(stats.totalSize / stats.scaleFactor);
            }

            context.connectionPooler->returnConnection(voidConnection);
        }
        else
        {
            /* Fallback to mock values */
            stats.collections = 1;
            stats.objects = 100;
            stats.dataSize = 51200;
            stats.storageSize = 102400;
            stats.avgObjSize = 512.0;
            stats.indexes = 2;
            stats.indexSize = 10240;
            stats.totalSize = 61440;
        }
    }
    catch (const std::exception& e)
    {
        /* Use fallback values on error */
        stats.collections = 0;
        stats.objects = 0;
        stats.dataSize = 0;
        stats.storageSize = 0;
        stats.avgObjSize = 0.0;
        stats.indexes = 0;
        stats.indexSize = 0;
        stats.totalSize = 0;
    }

    return stats;
}

int64_t CDbStatsCommand::getTableCount(const CommandContext& context)
{
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);

            string sql =
                "SELECT COUNT(*) FROM information_schema.tables WHERE "
                "table_schema = 'public' AND table_type = 'BASE TABLE'";
            auto result = connection->database->executeQuery(sql);

            context.connectionPooler->returnConnection(voidConnection);

            if (result.success && !result.rows.empty() &&
                !result.rows[0].empty())
            {
                return std::stoll(result.rows[0][0]);
            }
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback on error */
    }

    return 1; /* Default fallback */
}

int64_t CDbStatsCommand::getTotalRows(const CommandContext& context)
{
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);

            /* Get total row count across all user tables */
            string sql = "SELECT SUM(n_tup_ins + n_tup_upd + n_tup_del) FROM "
                         "pg_stat_user_tables";
            auto result = connection->database->executeQuery(sql);

            context.connectionPooler->returnConnection(voidConnection);

            if (result.success && !result.rows.empty() &&
                !result.rows[0].empty())
            {
                string value = result.rows[0][0];
                if (!value.empty() && value != "")
                {
                    return std::stoll(value);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback on error */
    }

    return 100; /* Default fallback */
}

int64_t CDbStatsCommand::getStorageSize(const CommandContext& context)
{
    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);

            /* Get total database size */
            string sql = "SELECT pg_database_size(current_database())";
            auto result = connection->database->executeQuery(sql);

            context.connectionPooler->returnConnection(voidConnection);

            if (result.success && !result.rows.empty() &&
                !result.rows[0].empty())
            {
                return std::stoll(result.rows[0][0]);
            }
        }
    }
    catch (const std::exception& e)
    {
        /* Fallback on error */
    }

    return 102400; /* Default fallback (100KB) */
}

double CDbStatsCommand::extractScale(const vector<uint8_t>& buffer,
                                     ssize_t bufferSize)
{
    /* Simple scale extraction - placeholder implementation */
    /* In a full implementation, this would parse the scale field from BSON */
    return 1.0; /* Default to no scaling */
}

CBsonType CDbStatsCommand::createStatsResponse(const DatabaseStats& stats)
{
    CBsonType response;
    response.initialize();
    response.beginDocument();

    response.addString("db", stats.db);
    response.addInt64("collections", stats.collections);
    response.addInt64("views", 0);
    response.addInt64("objects", stats.objects);
    response.addDouble("avgObjSize", stats.avgObjSize);
    response.addInt64("dataSize", stats.dataSize);
    response.addInt64("storageSize", stats.storageSize);
    response.addInt64("indexes", stats.indexes);
    response.addInt64("indexSize", stats.indexSize);
    response.addInt64("totalSize", stats.totalSize);
    response.addDouble("scaleFactor", stats.scaleFactor);

    response.endDocument();
    return response;
}

} // namespace FauxDB
