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
        std::cout << "[DEBUG] FauxDB FindCommand: No connection pooler provided" << std::endl;
    }
    else
    {
        std::cout << "[DEBUG] FauxDB FindCommand: Getting PostgreSQL connection..." << std::endl;
        auto pgConnection = connectionPooler->getPostgresConnection();
        std::cout << "[DEBUG] FauxDB FindCommand: Got connection: " << (pgConnection ? "YES" : "NO") << std::endl;
        
        if (pgConnection)
        {
            std::cout << "[DEBUG] FauxDB FindCommand: Database pointer: " << (pgConnection->database ? "YES" : "NO") << std::endl;
        }
        
        if (pgConnection && pgConnection->database)
        {
            try
            {
                stringstream sql;
                sql << "SELECT * FROM " << collection << " LIMIT " << DEFAULT_LIMIT;
                std::cout << "[DEBUG] FauxDB FindCommand: Executing SQL: " << sql.str() << std::endl;

                auto result = pgConnection->database->executeQuery(sql.str());
                std::cout << "[DEBUG] FauxDB FindCommand: Query success=" << result.success << ", rows returned=" << result.rows.size() << std::endl;

                if (result.success && !result.rows.empty())
                {
                    std::cout << "[DEBUG] FauxDB FindCommand: Converting rows to BSON..." << std::endl;
                    for (const auto& row : result.rows)
                    {
                        documents.push_back(
                            rowToBsonDocument(row, result.columnNames));
                    }
                    std::cout << "[DEBUG] FauxDB FindCommand: Converted " << documents.size() << " documents" << std::endl;
                }
            }
            catch (const std::exception& e)
            {
                std::cout << "[DEBUG] FauxDB FindCommand: Exception: " << e.what() << std::endl;
                /* Query failed - return empty results */
            }

            std::cout << "[DEBUG] FauxDB FindCommand: Releasing connection..." << std::endl;
            /* Always release the connection */
            connectionPooler->releasePostgresConnection(pgConnection);
            std::cout << "[DEBUG] FauxDB FindCommand: Connection released" << std::endl;
        }
        else
        {
            std::cout << "[DEBUG] FauxDB FindCommand: Failed to get database connection" << std::endl;
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
