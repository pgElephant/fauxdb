/*-------------------------------------------------------------------------
 *
 * CCreateIndexesCommand.hpp
 *      CreateIndexes command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

struct IndexSpec
{
    string name;
    string keyPattern;  /* JSON representation of key fields */
    bool unique;
    bool sparse;
    bool background;
    int32_t expireAfterSeconds;
    string partialFilterExpression;
};

class CCreateIndexesCommand : public CBaseCommand
{
public:
    CCreateIndexesCommand();
    virtual ~CCreateIndexesCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    vector<IndexSpec> extractIndexSpecs(const vector<uint8_t>& buffer, ssize_t bufferSize);
    string buildCreateIndexSQL(const string& collection, const IndexSpec& spec);
    string convertKeyPatternToSQL(const string& keyPattern);
    string generateIndexName(const string& collection, const IndexSpec& spec);
    bool indexExists(const CommandContext& context, const string& collection, const string& indexName);
};

} // namespace FauxDB
