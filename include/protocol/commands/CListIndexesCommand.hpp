/*-------------------------------------------------------------------------
 *
 * CListIndexesCommand.hpp
 *      ListIndexes command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

struct IndexInfo
{
    string name;
    string keyPattern;
    int32_t version;
    bool unique;
    bool sparse;
    string ns;  /* namespace */
};

class CListIndexesCommand : public CBaseCommand
{
public:
    CListIndexesCommand();
    virtual ~CListIndexesCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    vector<IndexInfo> getCollectionIndexes(const CommandContext& context, const string& collection);
    CBsonType createIndexInfoDocument(const IndexInfo& info);
    CBsonType createCursorResponse(const vector<IndexInfo>& indexes, const string& ns);
};

} // namespace FauxDB
