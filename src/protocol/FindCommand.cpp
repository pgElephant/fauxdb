/*-------------------------------------------------------------------------
 *
 * FindCommand.cpp
 *      MongoDB find command implementation.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

/* MongoDB Find Command Implementation */
#include "protocol/FindCommand.hpp"

#include "CLogMacros.hpp"

#include <iostream>
#include <sstream>

namespace FauxDB
{

vector<uint8_t>
FindCommand::execute(const string& collection, const vector<uint8_t>& buffer,
                     ssize_t bytesRead,
                     shared_ptr<CPGConnectionPooler> connectionPooler)
{
    CBsonType response = createBaseResponse();

    /* Create cursor response structure */
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    cursor.addInt64("id", 0);
    cursor.addString("ns", collection + ".collection");

    vector<CBsonType> documents;

    /* Try PostgreSQL integration */
    if (!connectionPooler)
    {
        debug_log("FauxDB FindCommand: No connection pooler provided");
    }
    else
    {
        debug_log("FauxDB FindCommand: Getting PostgreSQL connection...");
        auto pgConnection = connectionPooler->getPostgresConnection();
        debug_log("FauxDB FindCommand: Got connection: " +
                  string(pgConnection ? "YES" : "NO"));

        if (pgConnection)
        {
            debug_log("FauxDB FindCommand: Database pointer: " +
                      string(pgConnection->database ? "YES" : "NO"));
        }

        if (pgConnection && pgConnection->database)
        {
            try
            {
                stringstream sql;
                sql << "SELECT * FROM " << collection << " LIMIT "
                    << DEFAULT_LIMIT;
                debug_log("FauxDB FindCommand: Executing SQL: " + sql.str());

                auto result = pgConnection->database->executeQuery(sql.str());
                debug_log(
                    "FauxDB FindCommand: Query success=" +
                    string(result.success ? "true" : "false") +
                    ", rows returned=" + std::to_string(result.rows.size()));

                if (result.success && !result.rows.empty())
                {
                    debug_log("FauxDB FindCommand: Converting rows to BSON...");
                    for (const auto& row : result.rows)
                    {
                        documents.push_back(
                            rowToBsonDocument(row, result.columnNames));
                    }
                    debug_log("FauxDB FindCommand: Converted " +
                              std::to_string(documents.size()) + " documents");
                }
            }
            catch (const std::exception& e)
            {
                debug_log("FauxDB FindCommand: Exception: " + string(e.what()));
                /* Query failed - return empty results */
            }

            debug_log("FauxDB FindCommand: Releasing connection...");
            /* Always release the connection */
            connectionPooler->releasePostgresConnection(pgConnection);
            debug_log("FauxDB FindCommand: Connection released");
        }
        else
        {
            debug_log("FauxDB FindCommand: Failed to get database connection");
        }
    }

    /* Add documents to cursor firstBatch array */
    cursor.beginArray("firstBatch");
    for (const auto& doc : documents)
    {
        cursor.addArrayDocument(doc);
    }
    cursor.endArray();
    cursor.endDocument();

    response.addDocument("cursor", cursor);
    response.endDocument();

    return response.getDocument();
}

} /* namespace FauxDB */
