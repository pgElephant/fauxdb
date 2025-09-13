/*-------------------------------------------------------------------------
 *
 * CTcp.cpp
 *      TCP network communication handler for FauxDB.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "network/CTcp.hpp"

#include "database/CPGConnectionPooler.hpp"
#include "network/CThread.hpp"
#include "CLogger.hpp"
#include "CServerConfig.hpp"
#include "CLogMacros.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;
using namespace FauxDB;

CTcp::CTcp(const CServerConfig& config)
    : CNetwork(config), connectionPooler_(nullptr), logger_(std::make_shared<CLogger>(config))
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
        if (logger_)
            logger_->log(CLogLevel::DEBUG, "Creating socket for TCP server.");
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ == -1)
        {
            if (logger_)
                logger_->log(
                    CLogLevel::ERROR,
                    std::string("Failed to create socket. Error number: ") +
                        std::to_string(errno) + " (" + strerror(errno) + ").");
            return make_error_code(errc::connection_refused);
        }
        if (logger_)
            logger_->log(CLogLevel::INFO,
                         "Socket created successfully. File descriptor: " +
                             std::to_string(server_socket_) + ".");
        int opt = 1;
        // Unnecessary debug log removed.
        if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt,
                       sizeof(opt)) < 0)
        {
            if (logger_)
                logger_->log(
                    CLogLevel::ERROR,
                    std::string("Failed to set SO_REUSEADDR. Error number: ") +
                        std::to_string(errno) + " (" + strerror(errno) + ").");
            close(server_socket_);
            server_socket_ = -1;
            return make_error_code(errc::connection_refused);
        }
        if (logger_)
            logger_->log(CLogLevel::INFO, "SO_REUSEADDR set successfully.");
        initialized_ = true;
        if (logger_)
            logger_->log(CLogLevel::INFO,
                         "TCP server initialization completed successfully.");
        return error_code{};
    }
    catch (...)
    {
        return make_error_code(errc::connection_refused);
    }
}

error_code CTcp::start()
{
    if (logger_)
        logger_->log(CLogLevel::INFO, "Starting TCP server.");
    if (!isInitialized())
    {
        if (logger_)
            logger_->log(CLogLevel::ERROR, "TCP server is not initialized.");
        return make_error_code(errc::not_connected);
    }
    if (isRunning())
    {
        if (logger_)
            logger_->log(CLogLevel::INFO, "TCP server is already running.");
        return error_code{};
    }
    try
    {
        if (logger_)
            logger_->log(CLogLevel::INFO,
                         "Binding to address: '" + config_.bindAddress + ":" +
                             std::to_string(config_.port) + "'.");
        auto bindResult = bindToAddress(config_.bindAddress, config_.port);
        if (bindResult)
        {
            if (logger_)
                logger_->log(CLogLevel::ERROR,
                             "Bind failed: '" + bindResult.message() + "'.");
            return bindResult;
        }
        if (logger_)
            logger_->log(CLogLevel::INFO,
                         "Bind successful. Listening for connections.");
        if (listen(server_socket_, SOMAXCONN) < 0)
        {
            if (logger_)
                logger_->log(CLogLevel::ERROR,
                             std::string("Listen failed. Error number: ") +
                                 std::to_string(errno) + " (" +
                                 strerror(errno) + ").");
            return make_error_code(errc::connection_refused);
        }
        if (logger_)
            logger_->log(CLogLevel::INFO, "Listen successful.");
        running_ = true;
        listener_thread_ = thread(&CTcp::listenerLoop, this);
        if (logger_)
        {
            logger_->log(CLogLevel::INFO, "TCP listener started: address=" +
                                              config_.bindAddress + ", port=" +
                                              std::to_string(config_.port));
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

void CTcp::setConnectionPooler(shared_ptr<FauxDB::CPGConnectionPooler> pooler)
{
    lock_guard<mutex> lock(poolerMutex_);
    connectionPooler_ = pooler;
}

shared_ptr<FauxDB::CPGConnectionPooler> CTcp::getConnectionPooler() const
{
    lock_guard<mutex> lock(poolerMutex_);
    return connectionPooler_;
}

void CTcp::listenerLoop()
{
    if (logger_)
        logger_->log(CLogLevel::INFO, "Listener loop started.");
    while (running_.load())
    {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSocket =
            accept(server_socket_, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0)
        {
            // Suppress noisy errors on shutdown or expected aborts
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK ||
                errno == ECONNABORTED || !running_.load())
            {
                if (logger_)
                    logger_->log(
                        CLogLevel::INFO,
                        std::string("Accept interrupted or aborted (likely "
                                    "shutdown). Error number: ") +
                            std::to_string(errno) + " (" + strerror(errno) +
                            ").");
            }
            else
            {
                if (logger_)
                    logger_->log(CLogLevel::ERROR,
                                 std::string("Accept failed. Error number: ") +
                                     std::to_string(errno) + " (" +
                                     strerror(errno) + ").");
                if (logger_)
                {
                    logger_->log(CLogLevel::ERROR,
                                 "Failed to accept client connection: " +
                                     std::string(strerror(errno)));
                }
            }
            continue;
        }
        if (logger_)
        {
            logger_->log(CLogLevel::INFO,
                         "Accepted new client connection. Socket: '" +
                             std::to_string(clientSocket) + "'.");
        }

        std::thread([this, clientSocket]() { connectionWorker(clientSocket); })
            .detach();
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

    /* Set the logger on the document handler */
    if (logger_)
    {
        documentHandler->setLogger(logger_);
    }

    /* Create response builder */
    auto responseBuilder = make_unique<FauxDB::CBSONResponseBuilder>();

    vector<uint8_t> buffer(16384);
    debug_log("Client connected: " + std::to_string(clientSocket));

    while (running_.load())
    {
        // Read exactly 16 bytes for the header
        uint8_t headerBytes[16];
        ssize_t totalRead = 0;

        // Read header with retry on partial reads
        while (totalRead < 16)
        {
            ssize_t bytesRead =
                recv(clientSocket, headerBytes + totalRead, 16 - totalRead, 0);
            if (bytesRead <= 0)
            {
                std::cerr << "DEBUG: Client disconnected: " << clientSocket
                          << std::endl;
                goto cleanup; // Connection closed or error
            }
            totalRead += bytesRead;
        }

        // Extract message length (little endian)
        int32_t messageLength;
        std::memcpy(&messageLength, headerBytes, 4);
        std::cerr << "DEBUG: Message length: " << messageLength << std::endl;

        // Sanity check message length
        if (messageLength < 16 || messageLength > 48000000)
        {
            std::cerr << "DEBUG: Invalid message length: " << messageLength
                      << std::endl;
            break; // Invalid message length
        }

        // Ensure buffer is large enough
        if ((size_t)messageLength > buffer.size())
        {
            buffer.resize(messageLength);
        }

        // Copy header to buffer
        std::memcpy(buffer.data(), headerBytes, 16);

        // Read the rest of the message (messageLength - 16 bytes)
        totalRead = 16;
        ssize_t remainingBytes = messageLength - 16;

        while (remainingBytes > 0)
        {
            ssize_t bytesRead = recv(clientSocket, buffer.data() + totalRead,
                                     remainingBytes, 0);
            if (bytesRead <= 0)
            {
                std::cerr << "DEBUG: Connection error during body read"
                          << std::endl;
                goto cleanup; // Connection closed or error
            }
            totalRead += bytesRead;
            remainingBytes -= bytesRead;
        }

        // Assert: we read exactly messageLength bytes
        if (totalRead != messageLength)
        {
            std::cerr << "DEBUG: Read mismatch: expected " << messageLength
                      << ", got " << totalRead << std::endl;
            break; // Something went wrong
        }

        // Process complete message
        std::cerr << "DEBUG: Processing message of length: " << messageLength
                  << std::endl;
        auto response = documentHandler->processDocumentMessage(
            buffer, messageLength, *responseBuilder);
        std::cerr << "DEBUG: Got response from processDocumentMessage, size: "
                  << response.size() << std::endl;
        if (!response.empty())
        {
            std::cerr << "DEBUG: About to send response, size: "
                      << response.size() << std::endl;
            ssize_t bytesSent =
                send(clientSocket, response.data(), response.size(), 0);
            std::cerr << "DEBUG: Sent " << bytesSent
                      << " bytes (response size: " << response.size() << ")"
                      << std::endl;
        }
        else
        {
            std::cerr << "DEBUG: Response is empty, not sending" << std::endl;
        }
    }

cleanup:
    std::cerr << "DEBUG: Connection handler exiting for client: "
              << clientSocket << std::endl;

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
                         "Connection thread for client socket " +
                             std::to_string(clientSocket) +
                             " has been cleaned up.");
        }
    }
}

void CTcp::cleanupClosedConnections()
{
}
