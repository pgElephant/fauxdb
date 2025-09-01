/*-------------------------------------------------------------------------
 *
 * CListDatabasesCommand.hpp
 *      ListDatabases command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

struct DatabaseInfo
{
    string name;
    int64_t sizeOnDisk;
    bool empty;
};

class CListDatabasesCommand : public CBaseCommand
{
public:
    CListDatabasesCommand();
    virtual ~CListDatabasesCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    vector<DatabaseInfo> getDatabaseList(const CommandContext& context);
    int64_t getDatabaseSize(const CommandContext& context, const string& dbName);
    bool isDatabaseEmpty(const CommandContext& context, const string& dbName);
    bool extractNameOnly(const vector<uint8_t>& buffer, ssize_t bufferSize);
    CBsonType createDatabasesResponse(const vector<DatabaseInfo>& databases);
};

} // namespace FauxDB
