/*-------------------------------------------------------------------------
 *
 * CDropCommand.hpp
 *      Drop command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CDropCommand : public CBaseCommand
{
public:
    CDropCommand();
    virtual ~CDropCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    string buildDropTableSQL(const string& collectionName);
};

} // namespace FauxDB
