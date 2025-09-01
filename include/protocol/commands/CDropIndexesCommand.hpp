/*-------------------------------------------------------------------------
 *
 * CDropIndexesCommand.hpp
 *      DropIndexes command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CDropIndexesCommand : public CBaseCommand
{
public:
    CDropIndexesCommand();
    virtual ~CDropIndexesCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    string extractIndexSpecifier(const vector<uint8_t>& buffer, ssize_t bufferSize);
    vector<string> getIndexesToDrop(const CommandContext& context, const string& collection, const string& specifier);
    bool dropIndex(const CommandContext& context, const string& collection, const string& indexName);
    int32_t getIndexCount(const CommandContext& context, const string& collection);
};

} // namespace FauxDB
