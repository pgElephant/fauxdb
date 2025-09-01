/*-------------------------------------------------------------------------
 *
 * CCreateCommand.hpp
 *      Create command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CCreateCommand : public CBaseCommand
{
public:
    CCreateCommand();
    virtual ~CCreateCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    string buildCreateTableSQL(const string& collectionName);
    bool extractCappedOption(const vector<uint8_t>& buffer, ssize_t bufferSize);
    int64_t extractSizeOption(const vector<uint8_t>& buffer, ssize_t bufferSize);
    int64_t extractMaxOption(const vector<uint8_t>& buffer, ssize_t bufferSize);
};

} // namespace FauxDB
