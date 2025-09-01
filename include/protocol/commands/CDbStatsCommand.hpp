/*-------------------------------------------------------------------------
 *
 * CDbStatsCommand.hpp
 *      DbStats command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

struct DatabaseStats
{
    string db;
    int64_t collections;
    int64_t views;
    int64_t objects;
    double avgObjSize;
    int64_t dataSize;
    int64_t storageSize;
    int64_t indexes;
    int64_t indexSize;
    int64_t totalSize;
    double scaleFactor;
};

class CDbStatsCommand : public CBaseCommand
{
public:
    CDbStatsCommand();
    virtual ~CDbStatsCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    DatabaseStats collectDatabaseStats(const CommandContext& context);
    int64_t getTableCount(const CommandContext& context);
    int64_t getTotalRows(const CommandContext& context);
    int64_t getStorageSize(const CommandContext& context);
    double extractScale(const vector<uint8_t>& buffer, ssize_t bufferSize);
    CBsonType createStatsResponse(const DatabaseStats& stats);
};

} // namespace FauxDB
