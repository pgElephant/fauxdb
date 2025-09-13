/*-------------------------------------------------------------------------
 *
 * FindCommand.hpp
 *      MongoDB find command implementation.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

/* MongoDB Find Command */
#pragma once

#include "BaseCommand.hpp"

namespace FauxDB
{

/**
 * @brief MongoDB find command implementation
 */
class FindCommand : public BaseCommand
{
  private:
    static const int DEFAULT_LIMIT = 10;

  public:
    vector<uint8_t>
    execute(const string& collection, const vector<uint8_t>& buffer,
            ssize_t bytesRead,
            shared_ptr<CPGConnectionPooler> connectionPooler) override;

    string getCommandName() const override
    {
        return "find";
    }

  private:
    CBsonType rowToBsonDocument(const vector<string>& row, const vector<string>& columnNames);
};

} /* namespace FauxDB */
