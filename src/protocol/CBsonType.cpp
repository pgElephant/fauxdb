#include "CBsonType.hpp"

#include <cstring>
#include <sstream>

namespace FauxDB
{

CBsonType::CBsonType()
    : bsonDoc_(nullptr), bsonArray_(nullptr), hasErrors_(false), inArray_(false)
{
    bsonDoc_ = bson_new();
    if (!bsonDoc_)
    {
        setError("Failed to create BSON document");
    }
}

CBsonType::~CBsonType()
{
    if (bsonArray_)
    {
        bson_destroy(bsonArray_);
    }
    if (bsonDoc_)
    {
        bson_destroy(bsonDoc_);
    }
}

bool CBsonType::initialize()
{
    if (bsonDoc_)
    {
        bson_destroy(bsonDoc_);
    }
    bsonDoc_ = bson_new();
    if (!bsonDoc_)
    {
        setError("Failed to initialize BSON document");
        return false;
    }

    clearErrors();
    inArray_ = false;
    return true;
}

bool CBsonType::beginDocument()
{
    if (!checkBsonHandle())
    {
        return false;
    }
    return true;
}

bool CBsonType::endDocument()
{
    if (!checkBsonHandle())
    {
        return false;
    }
    return true;
}

bool CBsonType::beginArray(const std::string& key)
{
    if (!checkBsonHandle())
    {
        return false;
    }
    try
    {
        bsonArray_ = bson_new();
        if (!bsonArray_)
        {
            setError("Failed to create BSON array");
            return false;
        }
        inArray_ = true;
        currentArrayKey_ = key;
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to begin array: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::endArray()
{
    if (!checkBsonHandle() || !inArray_)
    {
        return false;
    }
    try
    {
        if (bsonArray_)
        {
            bson_destroy(bsonArray_);
            bsonArray_ = nullptr;
        }
        inArray_ = false;
        currentArrayKey_.clear();
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to end array: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::addString(const std::string& key, const std::string& value)
{
    if (!checkBsonHandle())
    {
        return false;
    }
    try
    {
        if (!bson_append_utf8(bsonDoc_, key.c_str(), -1, value.c_str(), -1))
        {
            setError("Failed to add string to BSON document");
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to add string: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::addInt32(const std::string& key, int32_t value)
{
    if (!checkBsonHandle())
    {
        return false;
    }
    try
    {
        if (!bson_append_int32(bsonDoc_, key.c_str(), -1, value))
        {
            setError("Failed to add int32 to BSON document");
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to add int32: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::addInt64(const std::string& key, int64_t value)
{
    if (!checkBsonHandle())
    {
        return false;
    }
    try
    {
        if (!bson_append_int64(bsonDoc_, key.c_str(), -1, value))
        {
            setError("Failed to add int64 to BSON document");
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to add int64: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::addDouble(const std::string& key, double value)
{
    if (!checkBsonHandle())
    {
        return false;
    }
    try
    {
        if (!bson_append_double(bsonDoc_, key.c_str(), -1, value))
        {
            setError("Failed to add double to BSON document");
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to add double: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::addBool(const std::string& key, bool value)
{
    if (!checkBsonHandle())
    {
        return false;
    }
    try
    {
        if (!bson_append_bool(bsonDoc_, key.c_str(), -1, value))
        {
            setError("Failed to add bool to BSON document");
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to add bool: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::addNull(const std::string& key)
{
    if (!checkBsonHandle())
    {
        return false;
    }
    try
    {
        if (!bson_append_null(bsonDoc_, key.c_str(), -1))
        {
            setError("Failed to add null to BSON document");
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to add null: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::addObjectId(const std::string& key, const std::string& objectId)
{
    if (!checkBsonHandle())
    {
        return false;
    }
    try
    {
        bson_oid_t oid;
        if (bson_oid_is_valid(objectId.c_str(), objectId.length()))
        {
            bson_oid_init_from_string(&oid, objectId.c_str());
            if (!bson_append_oid(bsonDoc_, key.c_str(), -1, &oid))
            {
                setError("Failed to add objectId to BSON document");
                return false;
            }
            return true;
        }
        else
        {
            setError("Invalid objectId format");
            return false;
        }
    }
    catch(const std::exception& e)
    {
        setError("Failed to add objectId: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::addDateTime(const std::string& key, int64_t timestamp)
{
    if (!checkBsonHandle())
    {
        return false;
    }
    try
    {
        if (!bson_append_date_time(bsonDoc_, key.c_str(), -1, timestamp))
        {
            setError("Failed to add datetime to BSON document");
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to add datetime: " + std::string(e.what()));
        return false;
    }
}

std::vector<uint8_t> CBsonType::getDocument() const
{
    if (!checkBsonHandle())
    {
        return std::vector<uint8_t>{};
    }
    try
    {
        // Return raw BSON bytes (little-endian), not JSON text.
        const uint8_t* data = static_cast<const uint8_t*>(bson_get_data(bsonDoc_));
        size_t len = bsonDoc_->len;
        std::vector<uint8_t> result;
        result.resize(len);
        std::memcpy(result.data(), data, len);
        return result;
    }
    catch(const std::exception& e)
    {
        setError("Failed to get document: " + std::string(e.what()));
        return std::vector<uint8_t>{};
    }
}

bson_t* CBsonType::getBsonHandle() const
{
    return bsonDoc_;
}

bool CBsonType::parseDocument(const std::vector<uint8_t>& data)
{
    try
    {
        if (bsonDoc_)
        {
            bson_destroy(bsonDoc_);
        }
        bsonDoc_ = bson_new_from_data(data.data(), data.size());
        if (!bsonDoc_)
        {
            setError("Failed to parse BSON data");
            return false;
        }
        clearErrors();
        return true;
    }
    catch(const std::exception& e)
    {
        setError("Failed to parse document: " + std::string(e.what()));
        return false;
    }
}

bool CBsonType::parseDocument(const uint8_t* data, size_t size)
{
    if (!data || size == 0)
    {
        setError("Invalid data or size");
        return false;
    }
    std::vector<uint8_t> vec(data, data + size);
    return parseDocument(vec);
}

bool CBsonType::isValidBson(const std::vector<uint8_t>& data) const
{
    try
    {
        bson_t* temp_doc = bson_new_from_data(data.data(), data.size());
        if (temp_doc)
        {
            bson_destroy(temp_doc);
            return true;
        }
        return false;
    }
    catch(...)
    {
        return false;
    }
}

bool CBsonType::isValidBson(const uint8_t* data, size_t size) const
{
    if (!data || size == 0)
    {
        return false;
    }
    std::vector<uint8_t> vec(data, data + size);
    return isValidBson(vec);
}

std::string CBsonType::toJson() const
{
    if (!checkBsonHandle())
    {
        return "";
    }
    try
    {
        char* json_str = bson_as_canonical_extended_json(bsonDoc_, nullptr);
        if (!json_str)
        {
            setError("Failed to convert BSON to JSON");
            return "";
        }
        
        std::string result(json_str);
        bson_free(json_str);
        return result;
    }
    catch(const std::exception& e)
    {
        setError("Failed to convert to JSON: " + std::string(e.what()));
        return "";
    }
}

std::string CBsonType::toJsonExtended() const
{
    return toJson(); // Simplified implementation
}

size_t CBsonType::getDocumentSize() const
{
    if (!checkBsonHandle())
    {
        return 0;
    }
    try
    {
        return bsonDoc_->len;
    }
    catch(const std::exception& e)
    {
        setError("Failed to get document size: " + std::string(e.what()));
        return 0;
    }
}

void CBsonType::clear()
{
    try
    {
        if (bsonArray_)
        {
            bson_destroy(bsonArray_);
            bsonArray_ = nullptr;
        }
        if (bsonDoc_)
        {
            bson_destroy(bsonDoc_);
        }
        bsonDoc_ = bson_new();
        clearErrors();
        inArray_ = false;
        currentArrayKey_.clear();
    }
    catch(const std::exception& e)
    {
        setError("Failed to clear document: " + std::string(e.what()));
    }
}

bool CBsonType::isEmpty() const
{
    if (!checkBsonHandle())
    {
        return true;
    }
    try
    {
        return bsonDoc_->len == 5; // Empty BSON document is 5 bytes
    }
    catch(const std::exception& e)
    {
        setError("Failed to check if empty: " + std::string(e.what()));
        return true;
    }
}

std::string CBsonType::getLastError() const
{
    return lastError_;
}

void CBsonType::clearErrors()
{
    lastError_.clear();
    hasErrors_ = false;
}

bool CBsonType::hasErrors() const
{
    return hasErrors_;
}

void CBsonType::setError(const std::string& error) const
{
    lastError_ = error;
    hasErrors_ = true;
}

bool CBsonType::checkBsonHandle() const
{
    return bsonDoc_ != nullptr && !hasErrors_;
}

} // namespace FauxDB
