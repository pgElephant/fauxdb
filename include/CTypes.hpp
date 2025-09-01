#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
using namespace std;
using namespace std::chrono;
namespace FauxDB
{

using namespace FauxDB;
using String = string;
using StringVector = vector<string>;
using StringMap = unordered_map<string, string>;
using ByteVector = vector<uint8_t>;
using Timestamp = steady_clock::time_point;

using BsonValue = variant<monostate, bool, int32_t, int64_t, double, string,
                          ByteVector, Timestamp, string>;

using BsonDocument = unordered_map<string, BsonValue>;

struct QueryFilter
{
    string field;
    string operator_;
    BsonValue value;

    QueryFilter() = default;
    QueryFilter(const string& f, const string& op, const BsonValue& v)
        : field(f), operator_(op), value(v)
    {
    }
};

struct QueryProjection
{
    string field;
    bool include;

    QueryProjection() = default;
    QueryProjection(const string& f, bool inc) : field(f), include(inc)
    {
    }
};

struct QuerySort
{
    string field;
    int direction;

    QuerySort() = default;
    QuerySort(const string& f, int dir) : field(f), direction(dir)
    {
    }
};

struct QueryResponse
{
    bool success;
    BsonDocument data;
    string errorMessage;
    int64_t cursorId;
    bool hasMore;

    QueryResponse() : success(false), cursorId(0), hasMore(false)
    {
    }
};

struct CommandResponse
{
    bool ok;
    BsonDocument response;
    string errorMessage;
    int errorCode;

    CommandResponse() : ok(false), errorCode(0)
    {
    }
};

struct NetworkAddress
{
    string host;
    uint16_t port;
    string protocol;

    NetworkAddress() : port(0)
    {
    }
    NetworkAddress(const string& h, uint16_t p, const string& proto = "tcp")
        : host(h), port(p), protocol(proto)
    {
    }
};

struct ConnectionInfo
{
    string id;
    NetworkAddress remote;
    Timestamp connectedAt;
    Timestamp lastActivity;
    bool isActive;

    ConnectionInfo()
        : connectedAt(steady_clock::now()), lastActivity(steady_clock::now()),
          isActive(true)
    {
    }
};

struct ProtocolMessage
{
    uint8_t type;
    uint32_t requestId;
    uint32_t responseTo;
    BsonDocument payload;
    Timestamp timestamp;

    ProtocolMessage()
        : type(0), requestId(0), responseTo(0), timestamp(steady_clock::now())
    {
    }
};

struct CQueryContext
{
    string database;
    string collection;
    string filter_json;
    string projection_json;
    string sort_json;
    int limit;
    int skip;
    bool explain;

    CQueryContext() : limit(0), skip(0), explain(false)
    {
    }
};

struct CQueryResult
{
    bool success;
    vector<vector<string>> data;
    vector<string> columns;
    string errorMessage;
    int rowsAffected;
    steady_clock::time_point timestamp;

    CQueryResult()
        : success(false), rowsAffected(0), timestamp(steady_clock::now())
    {
    }
};

struct CClient
{
    string id;
    string address;
    uint16_t port;
    steady_clock::time_point connectedAt;
    steady_clock::time_point lastActivity;
    bool isActive;

    CClient()
        : port(0), connectedAt(steady_clock::now()),
          lastActivity(steady_clock::now()), isActive(true)
    {
    }
};

struct CConfigData
{
    string configFile;
    unordered_map<string, string> settings;
    bool loaded;
    steady_clock::time_point lastModified;

    CConfigData() : loaded(false), lastModified(steady_clock::now())
    {
    }
};

struct CServerStats
{
    steady_clock::time_point startTime;
    size_t totalConnections;
    size_t activeConnections;
    size_t totalRequests;
    size_t successfulRequests;
    size_t failedRequests;
    size_t totalQueries;
    size_t successfulQueries;
    size_t failedQueries;
    std::chrono::milliseconds averageResponseTime;
    string version;
    std::chrono::milliseconds uptime;

    CServerStats()
        : startTime(steady_clock::now()), totalConnections(0),
          activeConnections(0), totalRequests(0), successfulRequests(0),
          failedRequests(0), totalQueries(0), successfulQueries(0),
          failedQueries(0), averageResponseTime(std::chrono::milliseconds(0))
    {
    }
};

struct Result
{
    bool success;
    string message;
    BsonDocument data;

    Result() : success(false)
    {
    }
    Result(bool s, const string& msg) : success(s), message(msg)
    {
    }
    Result(bool s, const string& msg, const BsonDocument& d)
        : success(s), message(msg), data(d)
    {
    }
};

struct Statistics
{
    uint64_t totalRequests;
    uint64_t successfulRequests;
    uint64_t failedRequests;
    double averageResponseTime;
    Timestamp startTime;

    Statistics()
        : totalRequests(0), successfulRequests(0), failedRequests(0),
          averageResponseTime(0.0), startTime(steady_clock::now())
    {
    }
};
} /* namespace FauxDB */
