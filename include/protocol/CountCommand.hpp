/* MongoDB Count Commands */
#pragma once

#include "BaseCommand.hpp"

namespace FauxDB
{

/**
 * @brief Base class for count-related commands
 */
class CountCommand : public BaseCommand
{
protected:
    /**
     * @brief Execute count query on PostgreSQL
     * @param collection Collection name
     * @param database PostgreSQL database connection
     * @return Count result
     */
    int64_t executeCountQuery(const string& collection, shared_ptr<CPostgresDatabase> database);

public:
    string getCommandName() const override { return "count"; }
};

/**
 * @brief MongoDB countDocuments command implementation
 */
class CountDocumentsCommand : public CountCommand
{
public:
    vector<uint8_t> execute(
        const string& collection,
        const vector<uint8_t>& buffer,
        ssize_t bytesRead,
        shared_ptr<CPGConnectionPooler> connectionPooler
    ) override;
    
    string getCommandName() const override { return "countDocuments"; }
};

/**
 * @brief MongoDB estimatedDocumentCount command implementation
 */
class EstimatedDocumentCountCommand : public CountCommand
{
public:
    vector<uint8_t> execute(
        const string& collection,
        const vector<uint8_t>& buffer,
        ssize_t bytesRead,
        shared_ptr<CPGConnectionPooler> connectionPooler
    ) override;
    
    string getCommandName() const override { return "estimatedDocumentCount"; }
};

} /* namespace FauxDB */
