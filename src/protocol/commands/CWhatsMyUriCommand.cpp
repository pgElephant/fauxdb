/*-------------------------------------------------------------------------
 *
 * CWhatsMyUriCommand.cpp
 *      WhatsMyUri command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CWhatsMyUriCommand.hpp"

#include <unistd.h>

namespace FauxDB
{

CWhatsMyUriCommand::CWhatsMyUriCommand()
{
    /* Constructor */
}

string CWhatsMyUriCommand::getCommandName() const
{
    return "whatsMyUri";
}

vector<uint8_t> CWhatsMyUriCommand::execute(const CommandContext& context)
{
    /* WhatsMyUri doesn't require database connection */
    return executeWithoutDatabase(context);
}

bool CWhatsMyUriCommand::requiresDatabase() const
{
    return false; /* WhatsMyUri should always work */
}

vector<uint8_t>
CWhatsMyUriCommand::executeWithDatabase(const CommandContext& context)
{
    /* Same as without database for whatsMyUri */
    return executeWithoutDatabase(context);
}

vector<uint8_t>
CWhatsMyUriCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Return client connection information */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    /* Client connection string */
    string clientUri = getClientConnectionString();
    bson.addString("you", clientUri);

    /* Success indicator */
    bson.addDouble("ok", 1.0);

    bson.endDocument();
    return bson.getDocument();
}

string CWhatsMyUriCommand::getClientConnectionString()
{
    /* In a real implementation, this would extract the client's IP and port
     * from the socket connection. For now, we'll return a localhost connection.
     */

    string hostname = getServerHostname();
    int32_t port = getServerPort();

    /* Mock client connection - in real implementation would get from socket */
    return "127.0.0.1:50000";
}

string CWhatsMyUriCommand::getServerHostname()
{
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return string(hostname);
    }
    return "localhost";
}

int32_t CWhatsMyUriCommand::getServerPort()
{
    /* Default MongoDB port - in real implementation would get from config */
    return 27018;
}

} // namespace FauxDB
