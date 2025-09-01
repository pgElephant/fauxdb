/*-------------------------------------------------------------------------
 *
 * CServerStatusCommand.hpp
 *      ServerStatus command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CServerStatusCommand : public CBaseCommand
{
  public:
    CServerStatusCommand();
    virtual ~CServerStatusCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);

    CBsonType createHostInfo();
    CBsonType createVersionInfo();
    CBsonType createProcessInfo();
    CBsonType createUptimeInfo();
    CBsonType createConnectionsInfo();
    CBsonType createNetworkInfo();
    CBsonType createMemoryInfo();
    CBsonType createMetricsInfo();
    CBsonType createStorageEngineInfo();
};

} // namespace FauxDB
