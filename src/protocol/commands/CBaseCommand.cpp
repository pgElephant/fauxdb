/*-------------------------------------------------------------------------
 *
 * CBaseCommand.cpp
 *      Base class implementation for document database commands
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CBaseCommand.hpp"

#include <cstring>

namespace FauxDB
{

vector<uint8_t> CBaseCommand::createSuccessResponse(double okValue)
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", okValue);
    bson.endDocument();
    return bson.getDocument();
}

vector<uint8_t> CBaseCommand::createErrorResponse(int errorCode,
                                                  const string& errorMessage)
{
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    bson.addDouble("ok", 0.0);
    bson.addInt32("code", errorCode);
    bson.addString("errmsg", errorMessage);
    bson.endDocument();
    return bson.getDocument();
}

string CBaseCommand::extractStringField(const vector<uint8_t>& buffer,
                                        ssize_t bufferSize,
                                        const string& fieldName)
{
    /* Simple BSON field extraction - placeholder implementation */
    return "";
}

int32_t CBaseCommand::extractInt32Field(const vector<uint8_t>& buffer,
                                        ssize_t bufferSize,
                                        const string& fieldName)
{
    /* Simple BSON field extraction - placeholder implementation */
    return 0;
}

string CBaseCommand::getCollectionFromContext(const CommandContext& context)
{
    return context.collectionName.empty() ? "test" : context.collectionName;
}

void CBaseCommand::addInferredType(CBsonType& bson, const string& fieldName,
                                   const string& value)
{
    /* Simple type inference for PostgreSQL result conversion */
    if (value == "true" || value == "false")
    {
        bson.addBool(fieldName, value == "true");
    }
    else if (value.find('.') != string::npos)
    {
        try
        {
            double doubleValue = std::stod(value);
            bson.addDouble(fieldName, doubleValue);
        }
        catch (const std::exception&)
        {
            bson.addString(fieldName, value);
        }
    }
    else
    {
        try
        {
            int64_t intValue = std::stoll(value);
            if (intValue >= -2147483648LL && intValue <= 2147483647LL)
            {
                bson.addInt32(fieldName, static_cast<int32_t>(intValue));
            }
            else
            {
                bson.addInt64(fieldName, intValue);
            }
        }
        catch (const std::exception&)
        {
            bson.addString(fieldName, value);
        }
    }
}

} // namespace FauxDB
