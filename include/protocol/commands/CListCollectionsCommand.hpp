/*-------------------------------------------------------------------------
 *
 * CListCollectionsCommand.hpp
 *      ListCollections command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

class CListCollectionsCommand : public CBaseCommand
{
public:
    CListCollectionsCommand();
    virtual ~CListCollectionsCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    string buildListTablesSQL();
    CBsonType createCollectionInfo(const string& collectionName, const string& collectionType);
    bool extractFilter(const vector<uint8_t>& buffer, ssize_t bufferSize);
};

} // namespace FauxDB
