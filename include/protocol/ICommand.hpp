/* MongoDB Command Interface */
#pragma once

#include "../database/CPGConnectionPooler.hpp"
#include "CBsonType.hpp"

#include <memory>
#include <string>
#include <vector>

using namespace std;

namespace FauxDB
{

/**
 * @brief Interface for all MongoDB commands
 */
class ICommand
{
  public:
    virtual ~ICommand() = default;

    /**
     * @brief Execute the MongoDB command
     * @param collection Collection name
     * @param buffer Raw BSON buffer
     * @param bytesRead Number of bytes read
     * @param connectionPooler PostgreSQL connection pooler
     * @return BSON response as vector of bytes
     */
    virtual vector<uint8_t>
    execute(const string& collection, const vector<uint8_t>& buffer,
            ssize_t bytesRead,
            shared_ptr<CPGConnectionPooler> connectionPooler) = 0;

    /**
     * @brief Get command name
     * @return Command name
     */
    virtual string getCommandName() const = 0;

    /**
     * @brief Check if command requires PostgreSQL connection
     * @return true if requires connection, false otherwise
     */
    virtual bool requiresConnection() const = 0;
};

using CommandPtr = shared_ptr<ICommand>;

} /* namespace FauxDB */
