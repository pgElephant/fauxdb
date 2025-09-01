#include "COpReplyHandler.hpp"

#include "CBsonType.hpp"
#include "CDocumentWireProtocol.hpp"

#include <cstring>
#include <stdexcept>
using namespace std;
using namespace FauxDB;

COpReplyHandler::COpReplyHandler()
{
}
vector<uint8_t>
COpReplyHandler::handleLegacyQuery(const vector<uint8_t>& message)
{
    string collection;
    vector<uint8_t> query;
    if (!parseLegacyQuery(message, collection, query))
    {
        OpReplyResponse response;
        response.responseFlags = 0;
        response.cursorID = 0;
        response.startingFrom = 0;
        response.numberReturned = 0;
        if (message.size() >= 16)
            return buildReply(response, 0);
        return vector<uint8_t>();
    }
    if (collection == "admin.$cmd")
    {
        bool isIsMaster = true;
        if (isIsMaster)
            return handleIsMasterQuery(message);
    }
    OpReplyResponse response;
    response.responseFlags = 0;
    response.cursorID = 0;
    response.startingFrom = 0;
    response.numberReturned = 0;
    int32_t requestID = 0;
    return buildReply(response, requestID);
}
vector<uint8_t>
COpReplyHandler::handleIsMasterQuery(const vector<uint8_t>& message)
{
    CBsonType bsonBuilder;
    if (!bsonBuilder.initialize())
        return vector<uint8_t>();
    if (!bsonBuilder.beginDocument())
        return vector<uint8_t>();
    if (!bsonBuilder.addDouble("ok", 1.0))
        return vector<uint8_t>();
    if (!bsonBuilder.addBool("isWritablePrimary", true))
        return vector<uint8_t>();
    if (!bsonBuilder.addBool("ismaster", true))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("minWireVersion", 0))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("maxWireVersion", 21))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("maxBsonObjectSize", 16777216))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("maxMessageSizeBytes", 48000000))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("maxWriteBatchSize", 100000))
        return vector<uint8_t>();
    if (!bsonBuilder.addString("compression", "none"))
        return vector<uint8_t>();
    if (!bsonBuilder.addInt32("logicalSessionTimeoutMinutes", 30))
        return vector<uint8_t>();
    if (!bsonBuilder.endDocument())
        return vector<uint8_t>();
    OpReplyResponse response;
    response.responseFlags = 0;
    response.cursorID = 0;
    response.startingFrom = 0;
    response.numberReturned = 1;
    response.documents.push_back(bsonBuilder.getDocument());
    return serializeReply(response, 0);
}
vector<uint8_t> COpReplyHandler::buildReply(const OpReplyResponse& response,
                                            int32_t requestID)
{
    return serializeReply(response, requestID);
}
bool COpReplyHandler::parseLegacyQuery(const vector<uint8_t>& message,
                                       string& collection,
                                       vector<uint8_t>& query)
{
    if (message.size() < 28)
        return false;
    const uint8_t* data = message.data() + 16;
    size_t remaining = message.size() - 16;
    if (remaining < 4)
        return false;
    data += 4;
    remaining -= 4;
    const char* collStr = reinterpret_cast<const char*>(data);
    size_t collLen = 0;
    while (collLen < remaining && data[collLen] != 0)
        collLen++;
    if (collLen >= remaining)
        return false;
    collection = string(collStr, collLen);
    data += collLen + 1;
    remaining -= collLen + 1;
    if (remaining < 8)
        return false;
    data += 8;
    remaining -= 8;
    if (remaining >= 4)
        query.clear();
    return true;
}
vector<uint8_t> COpReplyHandler::serializeReply(const OpReplyResponse& response,
                                                int32_t requestID)
{
    vector<uint8_t> data;
    size_t totalSize = 36;
    for (const auto& doc : response.documents)
        totalSize += doc.size();
    data.reserve(totalSize);
    data.resize(16);
    data.resize(data.size() + 20);
    uint8_t* fields = data.data() + 16;
    memcpy(fields + 0, &response.responseFlags, sizeof(int32_t));
    memcpy(fields + 4, &response.cursorID, sizeof(int64_t));
    memcpy(fields + 12, &response.startingFrom, sizeof(int32_t));
    memcpy(fields + 16, &response.numberReturned, sizeof(int32_t));
    for (const auto& doc : response.documents)
    {
        auto docData = serializeBsonDocument(doc);
        data.insert(data.end(), docData.begin(), docData.end());
    }
    return data;
}
vector<uint8_t>
COpReplyHandler::serializeBsonDocument(const vector<uint8_t>& doc)
{
    return doc;
}
