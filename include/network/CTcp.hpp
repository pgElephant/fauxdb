/*-------------------------------------------------------------------------
 *
 * CTcp.hpp
 *      TCP network communication handler for FauxDB.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */


#pragma once

#include "../CServerConfig.hpp"
#include "../database/CPGConnectionPooler.hpp"
#include "../protocol/CDocumentProtocolHandler.hpp"
#include "../protocol/CDocumentWireProtocol.hpp"
#include "../protocol/CResponseBuilder.hpp"
#include "CNetwork.hpp"
#include "CThread.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>
using namespace std;
using namespace std::chrono;

namespace FauxDB
{

using namespace FauxDB;

class CTcp : public CNetwork
{
  public:
    CTcp(const FauxDB::CServerConfig& config);
    ~CTcp() override;

    error_code initialize() override;
    error_code start() override;
    void stop() override;

    bool isRunning() const override;
    bool isInitialized() const override;

    void setConnectionPooler(shared_ptr<FauxDB::CPGConnectionPooler> pooler);
    shared_ptr<FauxDB::CPGConnectionPooler> getConnectionPooler() const;

  protected:
    void listenerLoop() override;
    void cleanupClosedConnections() override;

  private:
    shared_ptr<FauxDB::CPGConnectionPooler> connectionPooler_;
    mutable mutex poolerMutex_;
    map<int, unique_ptr<CThread>> connectionThreads_;
    mutex threadsMutex_;
    void handleNewConnection(int clientSocket);
    void connectionWorker(int clientSocket);
    void cleanupConnection(int clientSocket);
};
} /* namespace FauxDB */
