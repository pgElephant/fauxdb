/*-------------------------------------------------------------------------
 *
 * FindOneCommand.cpp
 *      MongoDB findOne command implementation.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

/* MongoDB FindOne Command Implementation */
#include "protocol/FindOneCommand.hpp"

#include <sstream>

namespace FauxDB
{

vector<uint8_t>
FindOneCommand::execute(const string& collection, const vector<uint8_t>& buffer,
                        ssize_t bytesRead,
                        shared_ptr<CPGConnectionPooler> connectionPooler)
{
    CBsonType response = createBaseResponse();

    /* Simple test response */
    response.addString("test", "modular_findOne_works");
    response.endDocument();
    return response.getDocument();

    /* Commented out original implementation

    // Try PostgreSQL integration */
    auto database = getConnection(connectionPooler);
    if (database)
    {
        try
        {
            stringstream sql;
            sql << "SELECT * FROM " << collection << " LIMIT 1";

            auto result = database->executeQuery(sql.str());

            if (result.success && !result.rows.empty())
            {
                /* Add all fields from first row to response */
                const auto& row = result.rows[0];
                bool hasId = false;

                for (size_t j = 0;
                     j < result.columnNames.size() && j < row.size(); ++j)
                {
                    const string& colName = result.columnNames[j];
                    const string& value = row[j];

                    if (colName == "_id" || colName == "id")
                    {
                        response.addString("_id", value);
                        hasId = true;
                    }
                    else
                    {
                        addInferredType(response, colName, value);
                    }
                }

                /* Add generated _id if not present */
                if (!hasId)
                {
                    response.addString("_id", "pg_generated_id");
                }
            }
            else
            {
                /* No results - MongoDB returns null for findOne with no results
                 */
                response.addNull("_id");
            }
        }
        catch (const std::exception& e)
        {
            /* Query failed - return null result */
            response.addNull("_id");
        }
    }
    else
    {
        /* No connection - return null result */
        response.addNull("_id");
    }

    /* Original implementation was here but commented out for testing */
    return response.getDocument();
}

} /* namespace FauxDB */
