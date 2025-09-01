/*-------------------------------------------------------------------------
 *
 * CHelloCommand.cpp
 *      Hello command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CHelloCommand.hpp"
#include <chrono>
#include <unistd.h>
#include <sys/utsname.h>

namespace FauxDB
{


CHelloCommand::CHelloCommand()
{
    /* Constructor */
}


string
CHelloCommand::getCommandName() const
{
    return "hello";
}


vector<uint8_t>
CHelloCommand::execute(const CommandContext& context)
{
    /* Hello doesn't require database connection */
    return executeWithoutDatabase(context);
}


bool
CHelloCommand::requiresDatabase() const
{
    return false; /* Hello should always work */
}


vector<uint8_t>
CHelloCommand::executeWithDatabase(const CommandContext& context)
{
    /* Same as without database for hello */
    return executeWithoutDatabase(context);
}


vector<uint8_t>
CHelloCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Comprehensive hello response matching MongoDB format */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    /* Core hello response fields */
    bson.addBool("ismaster", true);
    bson.addBool("isMaster", true); /* Legacy compatibility */
    bson.addString("msg", "isdbgrid");
    
    /* Topology information */
    CBsonType topologyInfo = createTopologyInfo();
    bson.addInt32("topologyVersion", 1);
    
    /* Host information */
    string hostname = getServerHostname();
    int32_t port = getServerPort();
    string hostPort = hostname + ":" + std::to_string(port);
    
    bson.addString("me", hostPort);
    
    /* Hosts array */
    bson.beginArray("hosts");
    bson.addArrayString(hostPort);
    bson.endArray();
    
    /* Server capabilities */
    bson.addInt32("maxBsonObjectSize", 16777216); /* 16MB */
    bson.addInt32("maxMessageSizeBytes", 48000000); /* 48MB */
    bson.addInt32("maxWriteBatchSize", 100000);
    
    /* Timing information */
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    bson.addInt64("localTime", timestamp);
    
    /* Wire protocol versions */
    bson.addInt32("minWireVersion", 0);
    bson.addInt32("maxWireVersion", 17); /* MongoDB 6.0+ */
    
    /* Read/Write concerns */
    bson.addBool("readOnly", false);
    
    /* Replication information */
    CBsonType replicationInfo = createReplicationInfo();
    bson.addBool("isReplicationEnabled", false);
    
    /* Connection and operation limits */
    bson.addDouble("connectionId", 1.0);
    
    /* Server identification */
    bson.addString("operationTime", "6746426f0000000000000000");
    
    /* Success indicator */
    bson.addDouble("ok", 1.0);
    
    bson.endDocument();
    return bson.getDocument();
}


CBsonType
CHelloCommand::createTopologyInfo()
{
    CBsonType topology;
    topology.initialize();
    topology.beginDocument();
    
    topology.addString("type", "Single");
    topology.addInt32("logicalSessionTimeoutMinutes", 30);
    
    topology.endDocument();
    return topology;
}


CBsonType
CHelloCommand::createReplicationInfo()
{
    CBsonType replication;
    replication.initialize();
    replication.beginDocument();
    
    replication.addBool("ismaster", true);
    replication.addBool("secondary", false);
    replication.addString("setName", "");
    replication.addInt32("setVersion", -1);
    
    replication.endDocument();
    return replication;
}


CBsonType
CHelloCommand::createHostInfo()
{
    CBsonType host;
    host.initialize();
    host.beginDocument();
    
    /* Get system information */
    struct utsname unameData;
    if (uname(&unameData) == 0)
    {
        host.addString("system", unameData.sysname);
        host.addString("hostname", unameData.nodename);
        host.addString("release", unameData.release);
        host.addString("machine", unameData.machine);
    }
    else
    {
        host.addString("system", "Unknown");
        host.addString("hostname", "localhost");
        host.addString("release", "Unknown");
        host.addString("machine", "Unknown");
    }
    
    host.endDocument();
    return host;
}


CBsonType
CHelloCommand::createTimingInfo()
{
    CBsonType timing;
    timing.initialize();
    timing.beginDocument();
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    timing.addInt64("localTime", timestamp);
    timing.addInt64("utcDateTime", timestamp);
    
    timing.endDocument();
    return timing;
}


string
CHelloCommand::getServerHostname()
{
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return string(hostname);
    }
    return "localhost";
}


int32_t
CHelloCommand::getServerPort()
{
    /* Default MongoDB port - in real implementation would get from config */
    return 27018;
}

} // namespace FauxDB
