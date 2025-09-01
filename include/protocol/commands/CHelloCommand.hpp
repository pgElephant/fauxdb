/*-------------------------------------------------------------------------
 *
 * CHelloCommand.hpp
 *      Hello command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CHelloCommand : public CBaseCommand
{
public:
    CHelloCommand();
    virtual ~CHelloCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    CBsonType createTopologyInfo();
    CBsonType createReplicationInfo();
    CBsonType createHostInfo();
    CBsonType createTimingInfo();
    string getServerHostname();
    int32_t getServerPort();
};

} // namespace FauxDB
