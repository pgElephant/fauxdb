/*-------------------------------------------------------------------------
 *
 * CServer.hpp
 *      Main server class for FauxDB.
 *      Initializes and manages all server components.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once
#include "CServerConfig.hpp"
#include "CLogger.hpp"
#include "database/CPGConnectionPooler.hpp"
#include "network/CTcp.hpp"
#include "protocol/CDocumentCommandHandler.hpp"
#include "protocol/CDocumentProtocolHandler.hpp"

#include <atomic>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <source_location>
#include <span>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace FauxDB
{

using namespace FauxDB;



class CServer
{
  public:
	CServer();
	virtual ~CServer();
// ...existing code...

	/* Server lifecycle */
	virtual bool initialize(const CServerConfig& config);
	virtual bool start();
	virtual void stop();
	virtual void shutdown();
	virtual bool isRunning() const;
	virtual CServerStatus getStatus() const;


	/* Server configuration */
	virtual void setConfig(const CServerConfig& config);
	virtual CServerConfig getConfig() const;
	virtual bool loadConfigFromFile(const std::string& configFile);
	virtual bool saveConfigToFile(const std::string& configFile) const;
	virtual bool validateConfig(const CServerConfig& config) const;

	/* Server monitoring */
	virtual CServerStats getStats() const;
	virtual void resetStats();
	virtual std::string getStatusReport() const;
	virtual bool healthCheck();
	virtual void enableMetrics(bool enabled);

	/* Server management */
	virtual bool reloadConfiguration();
	virtual bool restart();
	virtual bool pause();
	virtual bool resume();
	virtual void setMaintenanceMode(bool enabled);

	/* Component management */
	virtual bool initializeComponents();
	virtual bool startComponents();
	virtual void stopComponents();
	virtual void shutdownComponents();
	virtual bool validateComponents() const;

	/* Server events */
	virtual void setStartupCallback(std::function<void()> callback);
	virtual void setShutdownCallback(std::function<void()> callback);
	virtual void
	setErrorCallback(std::function<void(const std::string&)> callback);
	virtual void setStatusChangeCallback(
		std::function<void(CServerStatus, CServerStatus)> callback);

	/* Server information */
	virtual std::string getServerInfo() const;
	virtual std::string getVersion() const;
	virtual std::string getBuildInfo() const;

	/* Logger management */
	virtual void setLogger(std::shared_ptr<CLogger> logger);
	virtual std::vector<std::string> getComponentInfo() const;

	/* Database and network information */
	virtual std::string getDatabaseStatus() const;
	virtual std::string getNetworkStatus() const;
	virtual bool isDatabaseHealthy() const;
	virtual bool isNetworkHealthy() const;

	/* Server statistics */
	virtual std::string getServerStatistics() const;

	/* Error handling */
	virtual std::string getLastError() const;
	virtual void clearErrors();
	virtual bool hasErrors() const;
	virtual std::vector<std::string> getErrorLog() const;

	/* Error handling */
	virtual void setError(const std::string& error);

  protected:
	/* Server state */
	CServerConfig config_;
	CServerStats stats_;
	std::atomic<CServerStatus> status_;
	std::atomic<bool> running_;
	std::atomic<bool> maintenanceMode_;

	/* Server components - simplified to avoid incomplete type issues */
	// Component pointers will be added as needed when implementing full
	// functionality

	/* Network components */
	        std::shared_ptr<FauxDB::CPGConnectionPooler> connectionPooler_;
	std::unique_ptr<FauxDB::CTcp> tcpServer_;

	/* Protocol components */
	std::unique_ptr<FauxDB::CDocumentProtocolHandler>
		documentProtocolHandler_;
	std::unique_ptr<FauxDB::CDocumentCommandHandler>
		documentCommandHandler_;

	/* Server callbacks */
	std::function<void()> startupCallback_;
	std::function<void()> shutdownCallback_;
	std::function<void(const std::string&)> errorCallback_;
	std::function<void(CServerStatus, CServerStatus)> statusChangeCallback_;

	/* Server threads */
	std::thread mainThread_;
	std::vector<std::thread> workerThreads_;
	std::mutex serverMutex_;
	std::condition_variable serverCondition_;

	/* Logger */
	std::shared_ptr<CLogger> logger_;

	/* Protected utility methods */
	virtual void setStatus(CServerStatus newStatus);
	virtual void updateStats();
	virtual void logServerEvent(const std::string& event,
								const std::string& details);
	virtual void handleServerError(const std::string& error);

	/* Component initialization helpers */
	virtual bool initializeNetworkComponent();
	virtual bool initializeDatabaseComponent();
	virtual bool initializeProtocolComponent();
	virtual bool initializeParsingComponent();
	virtual bool initializeQueryComponent();
	virtual bool initializeResponseComponent();
	virtual bool initializeLoggingComponent();
	virtual bool initializeConfigurationComponent();

	/* Component lifecycle helpers */
	virtual bool startNetworkComponent();
	virtual bool startDatabaseComponent();
	virtual bool startProtocolComponent();
	virtual bool startParsingComponent();
	virtual bool startQueryComponent();
	virtual bool startResponseComponent();
	virtual bool startLoggingComponent();
	virtual bool startConfigurationComponent();

	virtual void stopNetworkComponent();
	virtual void stopDatabaseComponent();
	virtual void stopProtocolComponent();
	virtual void stopParsingComponent();
	virtual void stopQueryComponent();
	virtual void stopResponseComponent();
	virtual void stopLoggingComponent();
	virtual void stopConfigurationComponent();

	/* Server thread management */
	virtual void startMainThread();
	virtual void stopMainThread();
	virtual void startWorkerThreads();
	virtual void stopWorkerThreads();
	virtual void mainThreadFunction();
	virtual void workerThreadFunction(size_t workerId);

  private:
	/* Private state */
	std::string lastError_;
	std::vector<std::string> errorLog_;
	std::chrono::steady_clock::time_point lastErrorTime_;
	std::atomic<bool> metricsEnabled_;

	/* Private utility methods */
	void initializeDefaults();
	void cleanupState();

	/* Configuration helpers */
	bool parseConfigFile(const std::string& configFile);
	bool parseConfigurationLine(const std::string& line);
	bool validateConfigFile(const std::string& configFile) const;
	std::string buildConfigErrorMessage(const std::string& operation,
										const std::string& details) const;

	/* Component validation helpers */
	bool validateNetworkComponent() const;
	bool validateDatabaseComponent() const;
	bool validateProtocolComponent() const;
	bool validateParsingComponent() const;
	bool validateQueryComponent() const;
	bool validateResponseComponent() const;
	bool validateLoggingComponent() const;
	bool validateConfigurationComponent() const;

	/* Server monitoring helpers */
	void updateUptime();
	void updateResponseTime(std::chrono::milliseconds responseTime);
	void updateConnectionStats(bool connectionEstablished);
	void updateRequestStats(bool success);
	void updateQueryStats(bool success);

	/* Error handling helpers */
	void addErrorToLog(const std::string& error);
	void clearErrorLog();
	std::string buildErrorMessage(const std::string& operation,
								  const std::string& details) const;

	/* Utility methods */
	void initializeComponentPointers();
	void cleanupComponentPointers();
	bool waitForComponentStartup(std::chrono::milliseconds timeout);
	bool waitForComponentShutdown(std::chrono::milliseconds timeout);
};

} /* namespace FauxDB */
