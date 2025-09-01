

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>



#include "network/CTcp.hpp"
#include "database/CPGConnectionPooler.hpp"
#include "network/CThread.hpp"

using namespace std;
using namespace FauxDB;

CTcp::CTcp(const CServerConfig& config)
	: CNetwork(config), connectionPooler_(nullptr)
{
}


CTcp::~CTcp()
{
	stop();
}

error_code CTcp::initialize()
{
	if (isInitialized())
		return error_code{};
	try
	{
		std::cerr << "DEBUG: Creating socket for TCP server" << std::endl;
		server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
		if (server_socket_ == -1)
		{
			std::cerr << "DEBUG: Failed to create socket, errno=" << errno << " (" << strerror(errno) << ")" << std::endl;
			return make_error_code(errc::connection_refused);
		}
		std::cerr << "DEBUG: Socket created successfully, fd=" << server_socket_ << std::endl;
		int opt = 1;
		std::cerr << "DEBUG: Setting SO_REUSEADDR socket option" << std::endl;
		if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt,
					   sizeof(opt)) < 0)
		{
			std::cerr << "DEBUG: Failed to set SO_REUSEADDR, errno=" << errno << " (" << strerror(errno) << ")" << std::endl;
			close(server_socket_);
			server_socket_ = -1;
			return make_error_code(errc::connection_refused);
		}
		std::cerr << "DEBUG: SO_REUSEADDR set successfully" << std::endl;
		initialized_ = true;
		std::cerr << "DEBUG: TCP server initialization completed successfully" << std::endl;
		return error_code{};
	}
	catch (...)
	{
		return make_error_code(errc::connection_refused);
	}
}

error_code CTcp::start()
{
	std::cerr << "DEBUG: Starting TCP server" << std::endl;
	if (!isInitialized())
	{
		std::cerr << "DEBUG: TCP server not initialized" << std::endl;
		return make_error_code(errc::not_connected);
	}
	if (isRunning())
	{
		std::cerr << "DEBUG: TCP server already running" << std::endl;
		return error_code{};
	}
	try
	{
		std::cerr << "DEBUG: About to bind to address: " << config_.bindAddress << ":" << config_.port << std::endl;
		auto bindResult = bindToAddress(config_.bindAddress, config_.port);
		if (bindResult)
		{
			std::cerr << "DEBUG: Bind failed: " << bindResult.message() << std::endl;
			return bindResult;
		}
		std::cerr << "DEBUG: Bind successful, about to listen" << std::endl;
		if (listen(server_socket_, SOMAXCONN) < 0)
		{
			std::cerr << "DEBUG: Listen failed, errno=" << errno << " (" << strerror(errno) << ")" << std::endl;
			return make_error_code(errc::connection_refused);
		}
		std::cerr << "DEBUG: Listen successful" << std::endl;
		running_ = true;
		listener_thread_ = thread(&CTcp::listenerLoop, this);
		if (logger_)
		{
			logger_->log(
				CLogLevel::INFO,
				"TCP listener started: address=" + config_.bindAddress +
					", port=" + std::to_string(config_.port));
		}
		return error_code{};
	}
	catch (...)
	{
		return make_error_code(errc::connection_refused);
	}
}

void CTcp::stop()
{
	if (!isRunning())
		return;
	running_ = false;
	if (server_socket_ != -1)
	{
		shutdown(server_socket_, SHUT_RDWR);
		close(server_socket_);
		server_socket_ = -1;
	}
	if (listener_thread_.joinable())
		listener_thread_.join();
	{
		lock_guard<mutex> lock(threadsMutex_);
		for (auto& [socket, thread] : connectionThreads_)
		{
			if (thread && thread->isRunning())
			{
				thread->stop();
				thread->join();
			}
		}
		connectionThreads_.clear();
	}
	initialized_ = false;
}

bool CTcp::isRunning() const
{
	return running_.load();
}

bool CTcp::isInitialized() const
{
	return initialized_.load();
}

void CTcp::setConnectionPooler(
	shared_ptr<FauxDB::CPGConnectionPooler> pooler)
{
	lock_guard<mutex> lock(poolerMutex_);
	connectionPooler_ = pooler;
}

shared_ptr<FauxDB::CPGConnectionPooler>
CTcp::getConnectionPooler() const
{
		lock_guard<mutex> lock(poolerMutex_);
		return connectionPooler_;
}

void CTcp::listenerLoop()
{
		std::cerr << "DEBUG: Listener loop started" << std::endl;
		while (running_.load()) {
			sockaddr_in clientAddr{};
			socklen_t clientAddrLen = sizeof(clientAddr);
			std::cerr << "DEBUG: About to accept connection" << std::endl;
			int clientSocket = accept(server_socket_, (sockaddr*)&clientAddr, &clientAddrLen);
			if (clientSocket < 0) {
				std::cerr << "DEBUG: Accept failed, errno=" << errno << " (" << strerror(errno) << ")" << std::endl;
				/* Only log as error if it's not a normal interruption */
				if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
					if (logger_) {
						logger_->log(CLogLevel::ERROR,
									 "Failed to accept client connection: " + std::string(strerror(errno)));
					}
				}
				continue;
			}
			if (logger_) {
				logger_->log(CLogLevel::INFO,
							 "Accepted new client connection: socket=" + std::to_string(clientSocket));
			}

			std::thread([this, clientSocket]() { connectionWorker(clientSocket); }).detach();
		}
}

void CTcp::connectionWorker(int clientSocket)
{
	auto pooler = getConnectionPooler();
	if (!pooler)
	{
		if (logger_)
		{
			logger_->log(CLogLevel::ERROR,
						 "No connection pooler available for client socket=" +
							 std::to_string(clientSocket));
		}
		close(clientSocket);
		return;
	}

	if (logger_)
	{
		logger_->log(CLogLevel::INFO,
					 "Connection worker started for client socket=" +
						 std::to_string(clientSocket));
	}
	
	auto documentHandler = make_unique<FauxDB::CDocumentProtocolHandler>();
	if (!documentHandler->initialize())
	{
		if (logger_)
		{
			logger_->log(CLogLevel::ERROR,
						 "Failed to initialize document protocol handler for "
						 "client socket=" +
							 std::to_string(clientSocket));
		}
		close(clientSocket);
		return;
	}
	
	/* Set the connection pooler on the document handler */
	documentHandler->setConnectionPooler(pooler);
	
	/* Create dummy query translator and response builder */
	auto queryTranslator = make_unique<FauxDB::CQueryTranslator>(nullptr);
	auto responseBuilder = make_unique<FauxDB::CBSONResponseBuilder>();
	
	vector<uint8_t> buffer(16384);
	while (running_.load())
	{
		ssize_t bytesRead = recv(clientSocket, buffer.data(), buffer.size(), 0);
		if (bytesRead <= 0)
			break;
		auto response = documentHandler->processDocumentMessage(
			buffer, bytesRead, *queryTranslator, *responseBuilder);
		if (!response.empty())
			send(clientSocket, response.data(), response.size(), 0);
	}
	
	if (logger_)
	{
		logger_->log(CLogLevel::INFO,
					 "Connection worker finished for client socket=" +
						 std::to_string(clientSocket));
	}
	
	close(clientSocket);
	cleanupConnection(clientSocket);
}

void CTcp::cleanupConnection(int clientSocket)
{
	lock_guard<mutex> lock(threadsMutex_);
	auto it = connectionThreads_.find(clientSocket);
	if (it != connectionThreads_.end())
	{
		if (it->second && it->second->isRunning())
		{
			it->second->stop();
			it->second->join();
		}
		connectionThreads_.erase(it);
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG,
						 "Cleaned up connection thread for client socket=" +
							 std::to_string(clientSocket));
		}
	}
}

void CTcp::cleanupClosedConnections()
{

}

