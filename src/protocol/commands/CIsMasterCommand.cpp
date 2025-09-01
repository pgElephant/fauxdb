/*-------------------------------------------------------------------------
 *
 * CIsMasterCommand.cpp
 *      IsMaster command implementation for document database (legacy
 * compatibility)
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CIsMasterCommand.hpp"

#include <chrono>
#include <sys/utsname.h>
#include <unistd.h>

namespace FauxDB
{

CIsMasterCommand::CIsMasterCommand()
{
    /* Constructor */
}

string CIsMasterCommand::getCommandName() const
{
    return "isMaster";
}

vector<uint8_t> CIsMasterCommand::execute(const CommandContext& context)
{
    /* IsMaster doesn't require database connection */
    return executeWithoutDatabase(context);
}

bool CIsMasterCommand::requiresDatabase() const
{
    return false; /* IsMaster should always work */
}

vector<uint8_t>
CIsMasterCommand::executeWithDatabase(const CommandContext& context)
{
    /* Same as without database for isMaster */
    return executeWithoutDatabase(context);
}

vector<uint8_t>
CIsMasterCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Legacy isMaster response - similar to hello but with legacy fields */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    /* Core legacy response fields */
    bson.addBool("ismaster", true);
    bson.addString("msg", "isdbgrid");

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
    bson.addInt32("maxBsonObjectSize", 16777216);   /* 16MB */
    bson.addInt32("maxMessageSizeBytes", 48000000); /* 48MB */
    bson.addInt32("maxWriteBatchSize", 100000);

    /* Timing information */
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
    bson.addInt64("localTime", timestamp);

    /* Wire protocol versions */
    bson.addInt32("minWireVersion", 0);
    bson.addInt32("maxWireVersion", 17); /* MongoDB 6.0+ */

    /* Read/Write concerns */
    bson.addBool("readOnly", false);

    /* Replication information (legacy format) */
    bson.addBool("secondary", false);
    bson.addString("setName", "");
    bson.addInt32("setVersion", -1);
    bson.addBool("isReplicationEnabled", false);

    /* Connection information */
    bson.addDouble("connectionId", 1.0);

    /* Legacy deprecation warning */
    bson.addString("$clusterTime", "");
    bson.addString("operationTime", "6746426f0000000000000000");

    /* Success indicator */
    bson.addDouble("ok", 1.0);

    bson.endDocument();
    return bson.getDocument();
}

CBsonType CIsMasterCommand::createLegacyResponse()
{
    CBsonType response;
    response.initialize();
    response.beginDocument();

    response.addBool("ismaster", true);
    response.addString("msg", "isdbgrid");
    response.addBool("readOnly", false);

    response.endDocument();
    return response;
}

string CIsMasterCommand::getServerHostname()
{
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return string(hostname);
    }
    return "localhost";
}

int32_t CIsMasterCommand::getServerPort()
{
    /* Default MongoDB port - in real implementation would get from config */
    return 27018;
}

} // namespace FauxDB
