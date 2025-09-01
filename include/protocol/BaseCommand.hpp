/* Base MongoDB Command Implementation */
#pragma once

#include "../database/CPostgresDatabase.hpp"
#include "ICommand.hpp"

namespace FauxDB
{

/**
 * @brief Base class for MongoDB commands with common functionality
 */
class BaseCommand : public ICommand
{
  protected:
    /**
     * @brief Create a basic BSON response with ok field
     * @param success Success status
     * @return CBsonType with ok field
     */
    CBsonType createBaseResponse(bool success = true);

    /**
     * @brief Get PostgreSQL connection from pool
     * @param connectionPooler Connection pooler
     * @return PostgreSQL connection or nullptr
     */
    shared_ptr<CPostgresDatabase>
    getConnection(shared_ptr<CPGConnectionPooler> connectionPooler);

    /**
     * @brief Release PostgreSQL connection back to pool
     * @param connectionPooler Connection pooler
     * @param voidConnection Raw connection pointer
     */
    void releaseConnection(shared_ptr<CPGConnectionPooler> connectionPooler,
                           shared_ptr<void> voidConnection);

    /**
     * @brief Convert PostgreSQL row to BSON document
     * @param row PostgreSQL row data
     * @param columnNames Column names
     * @return CBsonType document
     */
    CBsonType rowToBsonDocument(const vector<string>& row,
                                const vector<string>& columnNames);

    /**
     * @brief Infer data type and add to BSON
     * @param bson BSON object to add to
     * @param fieldName Field name
     * @param value String value to convert
     */
    void addInferredType(CBsonType& bson, const string& fieldName,
                         const string& value);

  public:
    bool requiresConnection() const override
    {
        return true;
    }
};

} /* namespace FauxDB */
