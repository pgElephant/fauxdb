/*-------------------------------------------------------------------------
 *
 * CExplainCommand.hpp
 *      Explain command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CExplainCommand : public CBaseCommand
{
  public:
    CExplainCommand();
    virtual ~CExplainCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);

    string extractExplainedCommand(const vector<uint8_t>& buffer,
                                   ssize_t bufferSize);
    string extractVerbosity(const vector<uint8_t>& buffer, ssize_t bufferSize);
    CBsonType createExecutionStats();
    CBsonType createQueryPlanner(const string& collection,
                                 const string& command);
};

} // namespace FauxDB
