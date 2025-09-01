#pragma once

#include <bson/bson.h>
#include <cstdint>
#include <string>
#include <vector>

namespace FauxDB
{

class CBsonType
{
  public:
    CBsonType();
    ~CBsonType();

    bool initialize();

    bool beginDocument();
    bool endDocument();

    bool beginArray(const std::string& key);
    bool endArray();

    bool addString(const std::string& key, const std::string& value);
    bool addInt32(const std::string& key, int32_t value);
    bool addInt64(const std::string& key, int64_t value);
    bool addDouble(const std::string& key, double value);
    bool addBool(const std::string& key, bool value);
    bool addNull(const std::string& key);
    bool addObjectId(const std::string& key, const std::string& objectId);
    bool addDateTime(const std::string& key, int64_t timestamp);

    bool addBinary(const std::string& key, bson_subtype_t subtype,
                   const uint8_t* data, size_t size);

    bool addDocument(const std::string& key, const CBsonType& subdoc);

    // Array element adders, valid only between beginArray/endArray
    bool addArrayString(const std::string& value);
    bool addArrayInt32(int32_t value);
    bool addArrayInt64(int64_t value);
    bool addArrayDouble(double value);
    bool addArrayBool(bool value);
    bool addArrayNull();
    bool addArrayObjectId(const std::string& objectId);
    bool addArrayDateTime(int64_t timestamp);
    bool addArrayBinary(bson_subtype_t subtype, const uint8_t* data,
                        size_t size);
    bool addArrayDocument(const CBsonType& subdoc);

    std::vector<uint8_t> getDocument() const;
    bson_t* getBsonHandle() const;

    bool parseDocument(const std::vector<uint8_t>& data);
    bool parseDocument(const uint8_t* data, size_t size);

    bool isValidBson(const std::vector<uint8_t>& data) const;
    bool isValidBson(const uint8_t* data, size_t size) const;

    std::string toJson() const;         // canonical extended JSON
    std::string toJsonExtended() const; // relaxed extended JSON

    size_t getDocumentSize() const;

    void clear();
    bool isEmpty() const;

    std::string getLastError() const;
    void clearErrors();
    bool hasErrors() const;

  private:
    // one-level array builder
    std::string nextArrayIndexKey();

    bool checkBsonHandle() const;

    void setError(const std::string& error) const;

  private:
    bson_t* bsonDoc_;
    bson_t* bsonArray_;

    mutable std::string lastError_;
    mutable bool hasErrors_;
    bool inArray_;
    std::string currentArrayKey_;
    size_t currentArrayIndex_;
};

} // namespace FauxDB
