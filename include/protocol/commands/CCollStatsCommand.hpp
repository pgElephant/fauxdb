/*-------------------------------------------------------------------------
 *
 * CCollStatsCommand.hpp
 *      CollStats command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

struct CollectionStats
{
    string ns;
    int64_t size;
    int64_t count;
    int64_t avgObjSize;
    int64_t storageSize;
    int64_t totalIndexSize;
    int64_t indexSizes;
    bool capped;
    int64_t max;
    int64_t maxSize;
    double scaleFactor;
};

class CCollStatsCommand : public CBaseCommand
{
  public:
    CCollStatsCommand();
    virtual ~CCollStatsCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);

    CollectionStats collectCollectionStats(const CommandContext& context,
                                           const string& collection);
    int64_t getCollectionSize(const CommandContext& context,
                              const string& collection);
    int64_t getCollectionCount(const CommandContext& context,
                               const string& collection);
    int64_t getTableSize(const CommandContext& context,
                         const string& collection);
    double extractScale(const vector<uint8_t>& buffer, ssize_t bufferSize);
    CBsonType createStatsResponse(const CollectionStats& stats);
};

} // namespace FauxDB
