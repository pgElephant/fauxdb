/* Collection Name Parser Implementation */
#include "protocol/CollectionNameParser.hpp"
#include <cstring>

namespace FauxDB
{

string
CollectionNameParser::extractCollectionName(
    const vector<uint8_t>& buffer,
    ssize_t bytesRead,
    const string& commandName)
{
    if (buffer.size() < 4 || bytesRead < 4)
    {
        return "";
    }
    
    try
    {
        return parseBsonForCollection(buffer, 4, commandName); /* Skip document length */
    }
    catch (...)
    {
        return "";
    }
}


string
CollectionNameParser::parseBsonForCollection(
    const vector<uint8_t>& buffer,
    size_t offset,
    const string& commandName)
{
    while (offset < buffer.size())
    {
        if (offset >= buffer.size())
        {
            break;
        }
        
        uint8_t type = buffer[offset++];
        
        /* End of document */
        if (type == 0)
        {
            break;
        }
        
        /* Read field name */
        string fieldName;
        while (offset < buffer.size() && buffer[offset] != 0)
        {
            fieldName += static_cast<char>(buffer[offset++]);
        }
        
        if (offset >= buffer.size())
        {
            break;
        }
        
        offset++; /* Skip null terminator */
        
        /* Check if this field contains collection name */
        if (type == 2 && isCollectionField(fieldName, commandName)) /* String type */
        {
            if (offset + 4 <= buffer.size())
            {
                uint32_t stringLength;
                memcpy(&stringLength, &buffer[offset], 4);
                offset += 4;
                
                if (stringLength > 0 && offset + stringLength <= buffer.size())
                {
                    return readBsonString(buffer, offset, stringLength - 1); /* -1 for null terminator */
                }
            }
        }
        else
        {
            /* Skip field value based on type */
            switch (type)
            {
                case 1: /* Double */
                    offset += 8;
                    break;
                case 2: /* String */
                    if (offset + 4 <= buffer.size())
                    {
                        uint32_t stringLength;
                        memcpy(&stringLength, &buffer[offset], 4);
                        offset += 4 + stringLength;
                    }
                    else
                    {
                        return "";
                    }
                    break;
                case 8: /* Boolean */
                    offset += 1;
                    break;
                case 16: /* Int32 */
                    offset += 4;
                    break;
                case 18: /* Int64 */
                    offset += 8;
                    break;
                default:
                    /* Unknown type, stop parsing */
                    return "";
            }
        }
    }
    
    return "";
}


string
CollectionNameParser::readBsonString(
    const vector<uint8_t>& buffer,
    size_t offset,
    uint32_t stringLength)
{
    if (offset + stringLength > buffer.size())
    {
        return "";
    }
    
    string result;
    result.reserve(stringLength);
    
    for (uint32_t i = 0; i < stringLength; ++i)
    {
        result += static_cast<char>(buffer[offset + i]);
    }
    
    return result;
}


bool
CollectionNameParser::isCollectionField(const string& fieldName, const string& commandName)
{
    /* Command-specific collection field names */
    if (!commandName.empty())
    {
        if (fieldName == commandName)
        {
            return true;
        }
    }
    
    /* Generic collection field names */
    return fieldName == "collection" || 
           fieldName == "find" || 
           fieldName == "findOne" || 
           fieldName == "countDocuments" || 
           fieldName == "count" || 
           fieldName == "estimatedDocumentCount";
}

} /* namespace FauxDB */
