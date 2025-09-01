/* MongoDB FindOne Command */
#pragma once

#include "BaseCommand.hpp"

namespace FauxDB
{

/**
 * @brief MongoDB findOne command implementation
 */
class FindOneCommand : public BaseCommand
{
public:
    vector<uint8_t> execute(
        const string& collection,
        const vector<uint8_t>& buffer,
        ssize_t bytesRead,
        shared_ptr<CPGConnectionPooler> connectionPooler
    ) override;
    
    string getCommandName() const override { return "findOne"; }
};

} /* namespace FauxDB */
