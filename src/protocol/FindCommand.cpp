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
                /* Use simple SQL query and existing rowToBsonDocument method */
                debug_log("FauxDB FindCommand: Using simple SQL query with existing BSON conversion");
                
                stringstream sql;
                sql << "SELECT * FROM " << collection << " ORDER BY id LIMIT " << DEFAULT_LIMIT;
                
                debug_log("FauxDB FindCommand: Executing SQL: " + sql.str());

                auto result = pgConnection->database->executeQuery(sql.str());
                debug_log("FauxDB FindCommand: Query success=" +
                          string(result.success ? "true" : "false") +
                          ", rows returned=" + std::to_string(result.rows.size()));

                if (result.success && !result.rows.empty())
                {
                    debug_log("FauxDB FindCommand: Converting " + std::to_string(result.rows.size()) + " rows to BSON documents");
                    
                    for (const auto& row : result.rows)
                    {
                        documents.push_back(rowToBsonDocument(row, result.columnNames));
                    }
                    
                    debug_log("FauxDB FindCommand: Successfully created " + 
                              std::to_string(documents.size()) + " BSON documents");
                }
                else
                {
                    debug_log("FauxDB FindCommand: No data returned from PostgreSQL query");
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

    debug_log("FauxDB FindCommand: Response structure created with " + 
              std::to_string(documents.size()) + " documents");

    /* Return just the BSON document - let COpMsgHandler handle wire protocol formatting */
    debug_log("FauxDB FindCommand: Returning BSON response with " + 
              std::to_string(documents.size()) + " documents");

    return response.getDocument();
}

CBsonType FindCommand::rowToBsonDocument(const vector<string>& row, const vector<string>& columnNames)
{
    CBsonType doc;
    doc.initialize();
    doc.beginDocument();
    
    for (size_t i = 0; i < columnNames.size() && i < row.size(); ++i)
    {
        string fieldName = columnNames[i];
        string fieldValue = row[i];
        
        /* Convert field value to appropriate BSON type */
        if (fieldName == "id")
        {
            try
            {
                int id = std::stoi(fieldValue);
                doc.addInt32(fieldName, id);
            }
            catch (...)
            {
                doc.addString(fieldName, fieldValue);
            }
        }
        else if (fieldName == "department_id")
        {
            try
            {
                int deptId = std::stoi(fieldValue);
                doc.addInt32(fieldName, deptId);
            }
            catch (...)
            {
                doc.addString(fieldName, fieldValue);
            }
        }
        else
        {
            doc.addString(fieldName, fieldValue);
        }
    }
    
    doc.endDocument();
    return doc;
}

} /* namespace FauxDB */
