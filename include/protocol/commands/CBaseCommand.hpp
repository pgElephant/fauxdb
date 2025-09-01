/*-------------------------------------------------------------------------
 *
 * CBaseCommand.hpp
 *      Base class for document database command implementations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CBsonType.hpp"
#include "IDocumentCommand.hpp"

namespace FauxDB
{

class CBaseCommand : public IDocumentCommand
{
  public:
    virtual ~CBaseCommand() = default;

    bool requiresDatabase() const override
    {
        return false;
    }

  protected:
    /* Helper methods for BSON response creation */
    vector<uint8_t> createSuccessResponse(double okValue = 1.0);
    vector<uint8_t> createErrorResponse(int errorCode,
                                        const string& errorMessage);

    /* Helper methods for field extraction */
    string extractStringField(const vector<uint8_t>& buffer, ssize_t bufferSize,
                              const string& fieldName);
    int32_t extractInt32Field(const vector<uint8_t>& buffer, ssize_t bufferSize,
                              const string& fieldName);

    /* Collection name extraction from context */
    string getCollectionFromContext(const CommandContext& context);

    /* Type inference for PostgreSQL result conversion */
    void addInferredType(CBsonType& bson, const string& fieldName,
                         const string& value);
};

} // namespace FauxDB
