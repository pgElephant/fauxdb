/*-------------------------------------------------------------------------
 *
 * CBsonType.cpp
 *      BSON data type handling for MongoDB wire protocol.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "CBsonType.hpp"

#include <cstring>
#include <sstream>

namespace FauxDB
{

CBsonType::CBsonType()
    : bsonDoc_(nullptr), bsonArray_(nullptr), lastError_(), hasErrors_(false),
      inArray_(false), currentArrayKey_(), currentArrayIndex_(0)
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
        bson_destroy(bsonArray_);
    if (bsonDoc_)
        bson_destroy(bsonDoc_);
}

bool CBsonType::initialize()
{
    if (bsonDoc_)
        bson_destroy(bsonDoc_);

    bsonDoc_ = bson_new();
    if (!bsonDoc_)
    {
        setError("Failed to initialize BSON document");
        return false;
    }
    clearErrors();
    inArray_ = false;
    currentArrayKey_.clear();
    currentArrayIndex_ = 0;
    return true;
}

bool CBsonType::beginDocument()
{
    return checkBsonHandle();
}

bool CBsonType::endDocument()
{
    return checkBsonHandle();
}

bool CBsonType::beginArray(const std::string& key)
{
    if (!checkBsonHandle())
        return false;
    if (inArray_)
    {
        setError("Nested arrays not supported in this builder");
        return false;
    }

    bsonArray_ = bson_new();
    if (!bsonArray_)
    {
        setError("Failed to create BSON array");
        return false;
    }
    inArray_ = true;
    currentArrayKey_ = key;
    currentArrayIndex_ = 0;
    return true;
}

bool CBsonType::endArray()
{
    if (!checkBsonHandle())
        return false;
    if (!inArray_)
    {
        setError("endArray called without beginArray");
        return false;
    }

    // Append array to the document
    if (!bson_append_array(bsonDoc_, currentArrayKey_.c_str(), -1, bsonArray_))
    {
        setError("Failed to append array to document");
        return false;
    }

    bson_destroy(bsonArray_);
    bsonArray_ = nullptr;
    inArray_ = false;
    currentArrayKey_.clear();
    currentArrayIndex_ = 0;
    return true;
}

bool CBsonType::addString(const std::string& key, const std::string& value)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_utf8(bsonDoc_, key.c_str(), -1, value.c_str(), -1))
    {
        setError("Failed to add string");
        return false;
    }
    return true;
}

bool CBsonType::addInt32(const std::string& key, int32_t value)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_int32(bsonDoc_, key.c_str(), -1, value))
    {
        setError("Failed to add int32");
        return false;
    }
    return true;
}

bool CBsonType::addInt64(const std::string& key, int64_t value)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_int64(bsonDoc_, key.c_str(), -1, value))
    {
        setError("Failed to add int64");
        return false;
    }
    return true;
}

bool CBsonType::addDouble(const std::string& key, double value)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_double(bsonDoc_, key.c_str(), -1, value))
    {
        setError("Failed to add double");
        return false;
    }
    return true;
}

bool CBsonType::addBool(const std::string& key, bool value)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_bool(bsonDoc_, key.c_str(), -1, value))
    {
        setError("Failed to add bool");
        return false;
    }
    return true;
}

bool CBsonType::addNull(const std::string& key)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_null(bsonDoc_, key.c_str(), -1))
    {
        setError("Failed to add null");
        return false;
    }
    return true;
}

bool CBsonType::addObjectId(const std::string& key, const std::string& objectId)
{
    if (!checkBsonHandle())
        return false;

    bson_oid_t oid;
    if (!bson_oid_is_valid(objectId.c_str(), objectId.length()))
    {
        setError("Invalid ObjectId format");
        return false;
    }
    bson_oid_init_from_string(&oid, objectId.c_str());

    if (!bson_append_oid(bsonDoc_, key.c_str(), -1, &oid))
    {
        setError("Failed to add ObjectId");
        return false;
    }
    return true;
}

bool CBsonType::addDateTime(const std::string& key, int64_t timestamp)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_date_time(bsonDoc_, key.c_str(), -1, timestamp))
    {
        setError("Failed to add datetime");
        return false;
    }
    return true;
}

bool CBsonType::addBinary(const std::string& key, bson_subtype_t subtype,
                          const uint8_t* data, size_t size)
{
    if (!checkBsonHandle())
        return false;
    if (!data && size != 0)
    {
        setError("Binary data is null with nonzero size");
        return false;
    }

    if (!bson_append_binary(bsonDoc_, key.c_str(), -1, subtype, data, size))
    {
        setError("Failed to add binary");
        return false;
    }
    return true;
}

bool CBsonType::addRegex(const std::string& key, const std::string& pattern,
                         const std::string& options)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_regex(bsonDoc_, key.c_str(), -1, pattern.c_str(),
                           options.c_str()))
    {
        setError("Failed to add regex");
        return false;
    }
    return true;
}

bool CBsonType::addJavaScript(const std::string& key, const std::string& code)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_code(bsonDoc_, key.c_str(), -1, code.c_str()))
    {
        setError("Failed to add JavaScript code");
        return false;
    }
    return true;
}

bool CBsonType::addJavaScriptWithScope(const std::string& key,
                                       const std::string& code,
                                       const CBsonType& scope)
{
    if (!checkBsonHandle())
        return false;

    bson_t* scopeBson = scope.getBsonHandle();
    if (!scopeBson)
    {
        setError("Invalid scope document");
        return false;
    }

    if (!bson_append_code_with_scope(bsonDoc_, key.c_str(), -1, code.c_str(),
                                     scopeBson))
    {
        setError("Failed to add JavaScript code with scope");
        return false;
    }
    return true;
}

bool CBsonType::addSymbol(const std::string& key, const std::string& symbol)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_symbol(bsonDoc_, key.c_str(), -1, symbol.c_str(), -1))
    {
        setError("Failed to add symbol");
        return false;
    }
    return true;
}

bool CBsonType::addDbPointer(const std::string& key,
                             const std::string& collection,
                             const std::string& objectId)
{
    if (!checkBsonHandle())
        return false;

    bson_oid_t oid;
    if (!bson_oid_is_valid(objectId.c_str(), objectId.length()))
    {
        setError("Invalid ObjectId for DB pointer");
        return false;
    }
    bson_oid_init_from_string(&oid, objectId.c_str());

    if (!bson_append_dbpointer(bsonDoc_, key.c_str(), -1, collection.c_str(),
                               &oid))
    {
        setError("Failed to add DB pointer");
        return false;
    }
    return true;
}

bool CBsonType::addDecimal128(const std::string& key,
                              const std::string& decimal)
{
    if (!checkBsonHandle())
        return false;

    bson_decimal128_t dec;
    if (!bson_decimal128_from_string(decimal.c_str(), &dec))
    {
        setError("Invalid decimal128 string");
        return false;
    }

    if (!bson_append_decimal128(bsonDoc_, key.c_str(), -1, &dec))
    {
        setError("Failed to add decimal128");
        return false;
    }
    return true;
}

bool CBsonType::addMinKey(const std::string& key)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_minkey(bsonDoc_, key.c_str(), -1))
    {
        setError("Failed to add MinKey");
        return false;
    }
    return true;
}

bool CBsonType::addMaxKey(const std::string& key)
{
    if (!checkBsonHandle())
        return false;

    if (!bson_append_maxkey(bsonDoc_, key.c_str(), -1))
    {
        setError("Failed to add MaxKey");
        return false;
    }
    return true;
}

bool CBsonType::addDocument(const std::string& key, const CBsonType& subdoc)
{
    if (!checkBsonHandle())
        return false;
    if (!subdoc.getBsonHandle())
    {
        setError("Subdocument handle is null");
        return false;
    }
    if (!bson_append_document(bsonDoc_, key.c_str(), -1,
                              subdoc.getBsonHandle()))
    {
        setError("Failed to add embedded document");
        return false;
    }
    return true;
}

/* Array element helpers */

std::string CBsonType::nextArrayIndexKey()
{
    std::ostringstream oss;
    oss << currentArrayIndex_++;
    return oss.str();
}

bool CBsonType::addArrayString(const std::string& value)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayString used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_utf8(bsonArray_, k.c_str(), -1, value.c_str(), -1))
    {
        setError("Failed to add array string");
        return false;
    }
    return true;
}

bool CBsonType::addArrayInt32(int32_t value)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayInt32 used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_int32(bsonArray_, k.c_str(), -1, value))
    {
        setError("Failed to add array int32");
        return false;
    }
    return true;
}

bool CBsonType::addArrayInt64(int64_t value)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayInt64 used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_int64(bsonArray_, k.c_str(), -1, value))
    {
        setError("Failed to add array int64");
        return false;
    }
    return true;
}

bool CBsonType::addArrayDouble(double value)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayDouble used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_double(bsonArray_, k.c_str(), -1, value))
    {
        setError("Failed to add array double");
        return false;
    }
    return true;
}

bool CBsonType::addArrayBool(bool value)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayBool used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_bool(bsonArray_, k.c_str(), -1, value))
    {
        setError("Failed to add array bool");
        return false;
    }
    return true;
}

bool CBsonType::addArrayNull()
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayNull used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_null(bsonArray_, k.c_str(), -1))
    {
        setError("Failed to add array null");
        return false;
    }
    return true;
}

bool CBsonType::addArrayObjectId(const std::string& objectId)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayObjectId used outside array");
        return false;
    }

    if (!bson_oid_is_valid(objectId.c_str(), objectId.length()))
    {
        setError("Invalid ObjectId for array");
        return false;
    }
    bson_oid_t oid;
    bson_oid_init_from_string(&oid, objectId.c_str());

    std::string k = nextArrayIndexKey();
    if (!bson_append_oid(bsonArray_, k.c_str(), -1, &oid))
    {
        setError("Failed to add array ObjectId");
        return false;
    }
    return true;
}

bool CBsonType::addArrayDateTime(int64_t timestamp)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayDateTime used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_date_time(bsonArray_, k.c_str(), -1, timestamp))
    {
        setError("Failed to add array datetime");
        return false;
    }
    return true;
}

bool CBsonType::addArrayRegex(const std::string& pattern,
                              const std::string& options)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayRegex used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_regex(bsonArray_, k.c_str(), -1, pattern.c_str(),
                           options.c_str()))
    {
        setError("Failed to add array regex");
        return false;
    }
    return true;
}

bool CBsonType::addArrayJavaScript(const std::string& code)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayJavaScript used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_code(bsonArray_, k.c_str(), -1, code.c_str()))
    {
        setError("Failed to add array JavaScript code");
        return false;
    }
    return true;
}

bool CBsonType::addArrayJavaScriptWithScope(const std::string& code,
                                            const CBsonType& scope)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayJavaScriptWithScope used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    bson_t* scopeBson = scope.getBsonHandle();
    if (!scopeBson)
    {
        setError("Invalid scope document for array");
        return false;
    }
    if (!bson_append_code_with_scope(bsonArray_, k.c_str(), -1, code.c_str(),
                                     scopeBson))
    {
        setError("Failed to add array JavaScript code with scope");
        return false;
    }
    return true;
}

bool CBsonType::addArraySymbol(const std::string& symbol)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArraySymbol used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_symbol(bsonArray_, k.c_str(), -1, symbol.c_str(), -1))
    {
        setError("Failed to add array symbol");
        return false;
    }
    return true;
}

bool CBsonType::addArrayDbPointer(const std::string& collection,
                                  const std::string& objectId)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayDbPointer used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    bson_oid_t oid;
    if (!bson_oid_is_valid(objectId.c_str(), objectId.length()))
    {
        setError("Invalid ObjectId for array DB pointer");
        return false;
    }
    bson_oid_init_from_string(&oid, objectId.c_str());
    if (!bson_append_dbpointer(bsonArray_, k.c_str(), -1, collection.c_str(),
                               &oid))
    {
        setError("Failed to add array DB pointer");
        return false;
    }
    return true;
}

bool CBsonType::addArrayDecimal128(const std::string& decimal)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayDecimal128 used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    bson_decimal128_t dec;
    if (!bson_decimal128_from_string(decimal.c_str(), &dec))
    {
        setError("Invalid decimal128 string for array");
        return false;
    }
    if (!bson_append_decimal128(bsonArray_, k.c_str(), -1, &dec))
    {
        setError("Failed to add array decimal128");
        return false;
    }
    return true;
}

bool CBsonType::addArrayMinKey()
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayMinKey used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_minkey(bsonArray_, k.c_str(), -1))
    {
        setError("Failed to add array MinKey");
        return false;
    }
    return true;
}

bool CBsonType::addArrayMaxKey()
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayMaxKey used outside array");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_maxkey(bsonArray_, k.c_str(), -1))
    {
        setError("Failed to add array MaxKey");
        return false;
    }
    return true;
}

bool CBsonType::addArrayBinary(bson_subtype_t subtype, const uint8_t* data,
                               size_t size)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayBinary used outside array");
        return false;
    }
    if (!data && size != 0)
    {
        setError("Array binary data is null with nonzero size");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_binary(bsonArray_, k.c_str(), -1, subtype, data, size))
    {
        setError("Failed to add array binary");
        return false;
    }
    return true;
}

bool CBsonType::addArrayDocument(const CBsonType& subdoc)
{
    if (!inArray_ || !bsonArray_)
    {
        setError("addArrayDocument used outside array");
        return false;
    }
    if (!subdoc.getBsonHandle())
    {
        setError("Array subdocument handle is null");
        return false;
    }
    std::string k = nextArrayIndexKey();
    if (!bson_append_document(bsonArray_, k.c_str(), -1,
                              subdoc.getBsonHandle()))
    {
        setError("Failed to add array document");
        return false;
    }
    return true;
}

std::vector<uint8_t> CBsonType::getDocument() const
{
    if (!checkBsonHandle())
        return {};

    const uint8_t* data = static_cast<const uint8_t*>(bson_get_data(bsonDoc_));
    size_t len = bsonDoc_->len;

    std::vector<uint8_t> result(len);
    std::memcpy(result.data(), data, len);
    return result;
}

bson_t* CBsonType::getBsonHandle() const
{
    return bsonDoc_;
}

bool CBsonType::parseDocument(const std::vector<uint8_t>& data)
{
    if (bsonDoc_)
        bson_destroy(bsonDoc_);

    if (data.empty())
    {
        setError("Empty input for parseDocument");
        bsonDoc_ = bson_new(); // keep valid handle
        return false;
    }

    bsonDoc_ = bson_new_from_data(data.data(), data.size());
    if (!bsonDoc_)
    {
        setError("Failed to parse BSON data");
        return false;
    }
    clearErrors();
    inArray_ = false;
    if (bsonArray_)
    {
        bson_destroy(bsonArray_);
        bsonArray_ = nullptr;
    }
    currentArrayKey_.clear();
    currentArrayIndex_ = 0;
    return true;
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
    if (data.empty())
        return false;

    bson_t* temp = bson_new_from_data(data.data(), data.size());
    if (!temp)
        return false;

    bool ok = bson_validate(temp, BSON_VALIDATE_NONE, nullptr);
    bson_destroy(temp);
    return ok;
}

bool CBsonType::isValidBson(const uint8_t* data, size_t size) const
{
    if (!data || size == 0)
        return false;

    bson_t* temp = bson_new_from_data(data, size);
    if (!temp)
        return false;

    bool ok = bson_validate(temp, BSON_VALIDATE_NONE, nullptr);
    bson_destroy(temp);
    return ok;
}

std::string CBsonType::toJson() const
{
    if (!checkBsonHandle())
        return std::string();

    size_t len = 0;
    char* json = bson_as_canonical_extended_json(bsonDoc_, &len);
    if (!json)
    {
        setError("Failed to convert to canonical JSON");
        return std::string();
    }
    std::string s(json, len);
    bson_free(json);
    return s;
}

std::string CBsonType::toJsonExtended() const
{
    if (!checkBsonHandle())
        return std::string();

    size_t len = 0;
    char* json = bson_as_relaxed_extended_json(bsonDoc_, &len);
    if (!json)
    {
        setError("Failed to convert to relaxed JSON");
        return std::string();
    }
    std::string s(json, len);
    bson_free(json);
    return s;
}

size_t CBsonType::getDocumentSize() const
{
    if (!checkBsonHandle())
        return 0;
    return bsonDoc_->len;
}

void CBsonType::clear()
{
    if (bsonArray_)
    {
        bson_destroy(bsonArray_);
        bsonArray_ = nullptr;
    }
    if (bsonDoc_)
        bson_destroy(bsonDoc_);

    bsonDoc_ = bson_new();
    clearErrors();
    inArray_ = false;
    currentArrayKey_.clear();
    currentArrayIndex_ = 0;
}

bool CBsonType::isEmpty() const
{
    if (!checkBsonHandle())
        return true;

    // Empty BSON document is 5 bytes
    return bsonDoc_->len == 5;
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