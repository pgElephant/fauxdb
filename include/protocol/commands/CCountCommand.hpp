/*-------------------------------------------------------------------------
 *
 * CCountCommand.hpp
 *      Count command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CCountCommand : public CBaseCommand
{
  public:
    CCountCommand();
    virtual ~CCountCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);

    string extractQuery(const vector<uint8_t>& buffer, ssize_t bufferSize);
    int64_t extractLimit(const vector<uint8_t>& buffer, ssize_t bufferSize);
    int64_t extractSkip(const vector<uint8_t>& buffer, ssize_t bufferSize);
    string buildCountSQL(const string& collectionName,
                         const string& whereClause);
    string convertQueryToWhere(const string& query);
};

} // namespace FauxDB
