/*-------------------------------------------------------------------------
 *
 * CNetwork.cpp
 *      Abstract network management base class for FauxDB server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "network/CNetwork.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using namespace std;
using namespace FauxDB;

// FauxDB namespace removed for direct symbol usage

/*-------------------------------------------------------------------------
 * CNetwork implementation
 *-------------------------------------------------------------------------*/

CNetwork::CNetwork(const FauxDB::CServerConfig& config)
	: config_(config), running_(false), initialized_(false), server_socket_(-1)
{
	logger_ = std::make_shared<FauxDB::CLogger>(config);
	logger_->enableConsoleOutput(true);
	logger_->setLogLevel(CLogLevel::ERROR);
	logger_->initialize();
}

CNetwork::~CNetwork()
{
}

const FauxDB::CServerConfig& CNetwork::getConfig() const
{
	return config_;
}

error_code CNetwork::bindToAddress(const string& address, int port)
{

	if (server_socket_ == -1)
	{
		logger_->log(CLogLevel::ERROR,
					 "Network: Invalid server socket for bind (address=" +
						 address + ", port=" + std::to_string(port) + ")");
		return make_error_code(errc::connection_refused);
	}

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);

	if (inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr) <= 0)
	{
		logger_->log(CLogLevel::ERROR,
					 "Network: Invalid bind address '" + address +
						 "' (port=" + std::to_string(port) + ")");
		return make_error_code(errc::invalid_argument);
	}

	if (bind(server_socket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
	{
		logger_->log(CLogLevel::ERROR,
					 "Network: Failed to bind socket to " + address + ":" +
						 std::to_string(port) + ", errno=" +
						 std::to_string(errno) + " (" + strerror(errno) + ")");
		return make_error_code(errc::connection_refused);
	}

	return error_code{};
}

/* Add logger member to CNetwork */
