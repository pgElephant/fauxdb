/*-------------------------------------------------------------------------
 *
 * CFindAndModifyCommand.hpp
 *      FindAndModify command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CFindAndModifyCommand : public CBaseCommand
{
  public:
    CFindAndModifyCommand();
    virtual ~CFindAndModifyCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);

    string extractQuery(const vector<uint8_t>& buffer, ssize_t bufferSize);
    string extractUpdate(const vector<uint8_t>& buffer, ssize_t bufferSize);
    bool extractUpsert(const vector<uint8_t>& buffer, ssize_t bufferSize);
    bool extractReturnNew(const vector<uint8_t>& buffer, ssize_t bufferSize);
};

} // namespace FauxDB
