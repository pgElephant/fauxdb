/*-------------------------------------------------------------------------
 *
 * CAggregateCommand.hpp
 *      Aggregate command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBaseCommand.hpp"

namespace FauxDB
{

struct PipelineStage
{
    string type;        /* $match, $group, $sort, $limit, etc. */
    string operation;   /* JSON representation of the stage */
};

class CAggregateCommand : public CBaseCommand
{
public:
    CAggregateCommand();
    virtual ~CAggregateCommand() = default;
    
    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
    
    vector<PipelineStage> extractPipeline(const vector<uint8_t>& buffer, ssize_t bufferSize);
    string convertPipelineToSQL(const vector<PipelineStage>& pipeline, const string& collection);
    string handleMatchStage(const string& operation);
    string handleGroupStage(const string& operation);
    string handleSortStage(const string& operation);
    string handleLimitStage(const string& operation);
    string handleSkipStage(const string& operation);
    CBsonType createCursorResponse(const vector<vector<string>>& rows, const vector<string>& columnNames);
};

} // namespace FauxDB
