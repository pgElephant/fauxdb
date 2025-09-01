/*-------------------------------------------------------------------------
 *
 * IDocumentCommand.hpp
 *      Interface for document database command implementations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBsonType.hpp"
#include "database/CPGConnectionPooler.hpp"
#include <memory>
#include <string>
#include <vector>

namespace FauxDB
{

using std::string;
using std::vector;
using std::shared_ptr;

struct CommandContext
{
    string collectionName;
    string databaseName;
    vector<uint8_t> requestBuffer;
    ssize_t requestSize;
    int32_t requestID;
    shared_ptr<CPGConnectionPooler> connectionPooler;
};

class IDocumentCommand
{
public:
    virtual ~IDocumentCommand() = default;
    
    virtual string getCommandName() const = 0;
    virtual vector<uint8_t> execute(const CommandContext& context) = 0;
    virtual bool requiresDatabase() const = 0;
};

} // namespace FauxDB
