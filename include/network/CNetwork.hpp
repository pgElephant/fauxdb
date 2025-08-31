#pragma once
#include "../CLogger.hpp"
#include "../CServerConfig.hpp"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace FauxDB
{

class CNetwork
{
public:
	CNetwork(const CServerConfig& config);
	virtual ~CNetwork();

	virtual std::error_code initialize() = 0;
	virtual std::error_code start() = 0;
	virtual void stop() = 0;

	virtual bool isRunning() const = 0;
	virtual bool isInitialized() const = 0;

	const CServerConfig& getConfig() const;

protected:
	CServerConfig config_;
	std::shared_ptr<CLogger> logger_;
	std::atomic<bool> running_;
	std::atomic<bool> initialized_;
	int server_socket_;
	std::thread listener_thread_;
	std::error_code bindToAddress(const std::string& address, int port);
	virtual void listenerLoop() = 0;
	virtual void cleanupClosedConnections() = 0;
};

} // namespace FauxDB
