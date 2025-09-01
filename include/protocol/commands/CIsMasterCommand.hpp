/*-------------------------------------------------------------------------
 *
 * CIsMasterCommand.hpp
 *      IsMaster command implementation for document database (legacy
 * compatibility)
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CIsMasterCommand : public CBaseCommand
{
  public:
    CIsMasterCommand();
    virtual ~CIsMasterCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);

    /* Reuse hello command logic */
    CBsonType createLegacyResponse();
    string getServerHostname();
    int32_t getServerPort();
};

} // namespace FauxDB
