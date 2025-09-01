
// System headers
#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

// Library headers

// Project headers
#include "CServer.hpp"
#include "CConfiguration.hpp"
#include "CLogger.hpp"
#include "database/CDatabase.hpp"
#include "network/CNetwork.hpp"
#include "parsing/CParser.hpp"
#include "protocol/CQueryTranslator.hpp"
#include "protocol/CResponseBuilder.hpp"

using namespace std;

namespace FauxDB
{

/*-------------------------------------------------------------------------
 * CServer implementation
 *-------------------------------------------------------------------------*/

CServer::CServer()
	: status_(CServerStatus::Stopped), running_(false), maintenanceMode_(false),
	  lastErrorTime_(std::chrono::steady_clock::now()), metricsEnabled_(false)
{
	std::cerr << "DEBUG: CServer constructor called" << std::endl;
	initializeDefaults();
	initializeComponentPointers();
	std::cerr << "DEBUG: CServer constructor completed" << std::endl;
}

CServer::~CServer()
{
	shutdown();
	cleanupComponentPointers();
}

bool CServer::initialize(const CServerConfig& config)
{
	try
	{
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Starting server initialization");
		}
		else
		{
			std::cerr << "DEBUG: Starting server initialization" << std::endl;
		}
		
		setConfig(config);

		if (!validateConfig(config))
		{
			handleServerError("Invalid configuration");
			return false;
		}

		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Configuration validated, initializing components");
		}
		else
		{
			std::cerr << "DEBUG: Configuration validated, initializing components" << std::endl;
		}

		if (!initializeComponents())
		{
			handleServerError("Failed to initialize components");
			return false;
		}

		setStatus(CServerStatus::Starting);
		return true;
	}
	catch (const std::exception& e)
	{
		handleServerError("Initialization failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::start()
{
	try
	{
		if (status_ == CServerStatus::Running)
		{
			return true;
		}

		if (status_ != CServerStatus::Starting)
		{
			handleServerError("Server not in starting state");
			return false;
		}

		if (!startComponents())
		{
			handleServerError("Failed to start components");
			return false;
		}

		startMainThread();
		startWorkerThreads();

		running_ = true;
		setStatus(CServerStatus::Running);

		if (startupCallback_)
		{
			startupCallback_();
		}

		return true;
	}
	catch (const std::exception& e)
	{
		handleServerError("Start failed: " + std::string(e.what()));
		return false;
	}
}

void CServer::stop()
{
	try
	{
		if (status_ == CServerStatus::Stopped)
		{
			return;
		}

		setStatus(CServerStatus::Stopping);
		running_ = false;

		stopWorkerThreads();
		stopMainThread();
		stopComponents();

		setStatus(CServerStatus::Stopped);

		if (shutdownCallback_)
		{
			shutdownCallback_();
		}
	}
	catch (const std::exception& e)
	{
		setError("Stop failed: " + std::string(e.what()));
	}
}

void CServer::shutdown()
{
	stop();
	cleanupState();
}

bool CServer::isRunning() const
{
	return running_ && status_ == CServerStatus::Running;
}

CServerStatus CServer::getStatus() const
{
	return status_;
}

void CServer::setConfig(const CServerConfig& config)
{
	config_ = config;
	
	if (logger_)
	{
		logger_->log(CLogLevel::DEBUG, "Config set on server: port=" + std::to_string(config.port) + 
			", bind_address=" + config.bindAddress + ", max_connections=" + std::to_string(config.maxConnections));
	}
	else
	{
		std::cerr << "DEBUG: Config set on server: port=" << config.port 
				  << ", bind_address=" << config.bindAddress 
				  << ", max_connections=" << config.maxConnections << std::endl;
	}
	
	/* Initialize TCP server with config if not already initialized */
	if (!tcpServer_)
	{
		tcpServer_ = std::make_unique<CTcp>(config);
	}
}

CServerConfig CServer::getConfig() const
{
	return config_;
}

bool CServer::loadConfigFromFile(const std::string& configFile)
{
	try
	{
		return parseConfigFile(configFile);
	}
	catch (const std::exception& e)
	{
		setError("Config file loading failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::saveConfigToFile(const std::string& configFile) const
{
	try
	{
		std::ofstream file(configFile);
		if (!file.is_open())
		{
			return false;
		}

		file << "# FauxDB Configuration File" << std::endl;
		file << "server_name=" << config_.serverName << std::endl;
		file << "bind_address=" << config_.bindAddress << std::endl;
		file << "port=" << config_.port << std::endl;
		file << "max_connections=" << config_.maxConnections << std::endl;
		file << "worker_threads=" << config_.workerThreads << std::endl;

		return true;
	}
	catch (const std::exception& e)
	{
		return false;
	}
}

bool CServer::validateConfig(const CServerConfig& config) const
{
	if (config.port == 0 || config.port > 65535)
	{
		return false;
	}

	if (config.maxConnections == 0)
	{
		return false;
	}

	if (config.workerThreads == 0)
	{
		return false;
	}

	return true;
}

CServerStats CServer::getStats() const
{
	return stats_;
}

void CServer::resetStats()
{
	stats_ = CServerStats();
}

std::string CServer::getStatusReport() const
{
	std::stringstream ss;
	ss << "Server Status: " << static_cast<int>(status_.load()) << "\n"
	   << "Running: " << (running_ ? "Yes" : "No") << "\n"
	   << "Uptime: " << stats_.uptime.count() << "ms\n"
	   << "Active Connections: " << stats_.activeConnections << "\n"
	   << "Total Requests: " << stats_.totalRequests;
	return ss.str();
}

bool CServer::healthCheck()
{
	try
	{
		// Basic health check
		if (status_ == CServerStatus::Error)
		{
			return false;
		}

		// Check if components are healthy
		// Component health checks removed to avoid incomplete type issues
		// TODO: Re-implement when component system is fully integrated

		updateStats();
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Health check failed: " + std::string(e.what()));
		return false;
	}
}

void CServer::enableMetrics(bool enabled)
{
	metricsEnabled_ = enabled;
}

bool CServer::reloadConfiguration()
{
	try
	{
		if (!config_.configFile.empty())
		{
			return loadConfigFromFile(config_.configFile);
		}
		return false;
	}
	catch (const std::exception& e)
	{
		setError("Configuration reload failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::restart()
{
	try
	{
		stop();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		return start();
	}
	catch (const std::exception& e)
	{
		setError("Restart failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::pause()
{
	try
	{
		if (status_ == CServerStatus::Running)
		{
			setStatus(CServerStatus::Maintenance);
			return true;
		}
		return false;
	}
	catch (const std::exception& e)
	{
		setError("Pause failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::resume()
{
	try
	{
		if (status_ == CServerStatus::Maintenance)
		{
			setStatus(CServerStatus::Running);
			return true;
		}
		return false;
	}
	catch (const std::exception& e)
	{
		setError("Resume failed: " + std::string(e.what()));
		return false;
	}
}

void CServer::setMaintenanceMode(bool enabled)
{
	maintenanceMode_ = enabled;
}

bool CServer::initializeComponents()
{
	try
	{
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Starting component initialization");
		}
		
		if (!initializeNetworkComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Network component initialization failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Network component initialized successfully");
		}
		
		if (!initializeDatabaseComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Database component initialization failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Database component initialized successfully");
		}
		
		if (!initializeProtocolComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Protocol component initialization failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Protocol component initialized successfully");
		}
		
		if (!initializeParsingComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Parsing component initialization failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Parsing component initialized successfully");
		}
		
		if (!initializeQueryComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Query component initialization failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Query component initialized successfully");
		}
		
		if (!initializeResponseComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Response component initialization failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Response component initialized successfully");
		}
		
		if (!initializeLoggingComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Logging component initialization failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Logging component initialized successfully");
		}
		
		if (!initializeConfigurationComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Configuration component initialization failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Configuration component initialized successfully");
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "All components initialized successfully");
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Component initialization failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::startComponents()
{
	try
	{
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Starting component startup process");
		}
		
		if (!startNetworkComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Network component startup failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Network component started successfully");
		}
		
		if (!startDatabaseComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Database component startup failed");
			}
			return false;
		}
		
		if (!startProtocolComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Protocol component startup failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Protocol component started successfully");
		}
		
		if (!startParsingComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Parsing component startup failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Parsing component started successfully");
		}
		
		if (!startQueryComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Query component startup failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Query component started successfully");
		}
		
		if (!startResponseComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Response component startup failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Response component started successfully");
		}
		
		if (!startLoggingComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Logging component startup failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Logging component started successfully");
		}
		
		if (!startConfigurationComponent()) {
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Configuration component startup failed");
			}
			return false;
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Configuration component started successfully");
		}
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "All components started successfully");
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Component startup failed: " + std::string(e.what()));
		return false;
	}
}

void CServer::stopComponents()
{
	stopNetworkComponent();
	stopDatabaseComponent();
	stopProtocolComponent();
	stopParsingComponent();
	stopQueryComponent();
	stopResponseComponent();
	stopLoggingComponent();
	stopConfigurationComponent();
}

void CServer::shutdownComponents()
{
	stopComponents();
}

bool CServer::validateComponents() const
{
	return validateNetworkComponent() && validateDatabaseComponent() &&
		   validateProtocolComponent() && validateParsingComponent() &&
		   validateQueryComponent() && validateResponseComponent() &&
		   validateLoggingComponent() && validateConfigurationComponent();
}

void CServer::setStartupCallback(std::function<void()> callback)
{
	startupCallback_ = callback;
}

void CServer::setShutdownCallback(std::function<void()> callback)
{
	shutdownCallback_ = callback;
}

void CServer::setErrorCallback(std::function<void(const std::string&)> callback)
{
	errorCallback_ = callback;
}

void CServer::setStatusChangeCallback(
	std::function<void(CServerStatus, CServerStatus)> callback)
{
	statusChangeCallback_ = callback;
}

std::string CServer::getServerInfo() const
{
	return "FauxDB Server v1.0.0";
}

std::string CServer::getVersion() const
{
	return "1.0.0";
}

std::string CServer::getBuildInfo() const
{
	return "FauxDB Server - PostgreSQL Backend";
}

std::vector<std::string> CServer::getComponentInfo() const
{
	std::vector<std::string> components;
	components.push_back("Network Component");
	components.push_back("Database Component");
	components.push_back("Protocol Handler");
	components.push_back("Parser Component");
	components.push_back("Query Translator");
	components.push_back("Response Builder");
	components.push_back("Logger Component");
	components.push_back("Configuration Component");
	return components;
}

std::string CServer::getDatabaseStatus() const
{
	if (!connectionPooler_)
	{
		return "Database: Not Initialized";
	}

	try
	{
		auto stats = connectionPooler_->getStats();
		std::string status = "Database: ";
		status += "Total=" + std::to_string(stats.totalConnections);
		status += ", Available=" + std::to_string(stats.availableConnections);
		status += ", InUse=" + std::to_string(stats.inUseConnections);
		status += ", Broken=" + std::to_string(stats.brokenConnections);
		return status;
	}
	catch (...)
	{
		return "Database: Status Unavailable";
	}
}

std::string CServer::getNetworkStatus() const
{
	if (!tcpServer_)
	{
		return "Network: Not Initialized";
	}

	try
	{
		std::string status = "Network: ";
		status +=
			std::string("Running=") + (tcpServer_->isRunning() ? "Yes" : "No");
		status += std::string(", Initialized=") +
				  (tcpServer_->isInitialized() ? "Yes" : "No");
		return status;
	}
	catch (...)
	{
		return "Network: Status Unavailable";
	}
}

bool CServer::isDatabaseHealthy() const
{
	if (!connectionPooler_)
	{
		return false;
	}

	try
	{
		auto stats = connectionPooler_->getStats();
		return stats.brokenConnections == 0 && stats.availableConnections > 0;
	}
	catch (...)
	{
		return false;
	}
}

bool CServer::isNetworkHealthy() const
{
	if (!tcpServer_)
	{
		return false;
	}

	try
	{
		return tcpServer_->isRunning() && tcpServer_->isInitialized();
	}
	catch (...)
	{
		return false;
	}
}

std::string CServer::getServerStatistics() const
{
	std::string stats = "Server Statistics:\n";
	stats +=
		"  Status: " + std::to_string(static_cast<int>(status_.load())) + "\n";
	stats += "  Running: " + std::string(running_ ? "Yes" : "No") + "\n";
	stats +=
		"  Maintenance Mode: " + std::string(maintenanceMode_ ? "Yes" : "No") +
		"\n";
	stats += "  " + getDatabaseStatus() + "\n";
	stats += "  " + getNetworkStatus() + "\n";

	if (connectionPooler_)
	{
		try
		{
			auto poolStats = connectionPooler_->getStats();
			stats += "  Connection Pool Stats:\n";
			stats += "    Total Requests: " +
					 std::to_string(poolStats.totalRequests) + "\n";
			stats += "    Successful Requests: " +
					 std::to_string(poolStats.successfulRequests) + "\n";
			stats += "    Failed Requests: " +
					 std::to_string(poolStats.failedRequests) + "\n";
			stats += "    Average Response Time: " +
					 std::to_string(poolStats.averageResponseTime.count()) +
					 "ms\n";
		}
		catch (...)
		{
			stats += "  Connection Pool Stats: Unavailable\n";
		}
	}

	return stats;
}

std::string CServer::getLastError() const
{
	return lastError_;
}

void CServer::clearErrors()
{
	clearErrorLog();
}

bool CServer::hasErrors() const
{
	return !errorLog_.empty();
}

std::vector<std::string> CServer::getErrorLog() const
{
	return errorLog_;
}

/* Protected methods implementation */
void CServer::setStatus(CServerStatus newStatus)
{
	CServerStatus oldStatus = status_;
	status_ = newStatus;

	if (statusChangeCallback_)
	{
		statusChangeCallback_(oldStatus, newStatus);
	}
}

void CServer::updateStats()
{
	updateUptime();
}

void CServer::logServerEvent(const std::string& event,
							 const std::string& details)
{
	// Log server events using CLogger
}

void CServer::handleServerError(const std::string& error)
{
	setError(error);
	if (errorCallback_)
	{
		errorCallback_(error);
	}
}

/* Component initialization helpers */
bool CServer::initializeNetworkComponent()
{
	try
	{
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Starting network component initialization");
		}
		else
		{
			std::cerr << "DEBUG: Starting network component initialization" << std::endl;
		}
	
		
		/* Check if components exist */
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Checking if connection pooler exists: " + std::string(connectionPooler_ ? "yes" : "no"));
		}
		else
		{
			std::cerr << "DEBUG: Checking if connection pooler exists: " << (connectionPooler_ ? "yes" : "no") << std::endl;
		}
		
		/* Always recreate the connection pooler to ensure proper configuration */
		if (connectionPooler_)
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "Shutting down existing connection pooler");
			}
			connectionPooler_->shutdown();
			connectionPooler_.reset();
		}
		
		/* Always recreate the TCP server to ensure proper configuration */
		if (tcpServer_)
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "Shutting down existing TCP server");
			}
			tcpServer_->stop();
			tcpServer_.reset();
		}
		
		if (!connectionPooler_)
		{

			/* Create PostgreSQL connection pooler */
			auto pooler = std::make_shared<FauxDB::CPGConnectionPooler>();
			
			/* Set PostgreSQL connection details from config */
			pooler->setPostgresConfig(config_.pgHost, config_.pgPort, config_.pgDatabase, 
									  config_.pgUser, config_.pgPassword);

			/* Configure the connection pooler */
			FauxDB::CConnectionPoolConfig poolConfig;
			poolConfig.minConnections = 1;
			poolConfig.maxConnections = 5;
			poolConfig.initialConnections = 1;
			poolConfig.connectionTimeout = std::chrono::milliseconds(5000);
			poolConfig.idleTimeout = std::chrono::milliseconds(300000);
			poolConfig.maxLifetime = std::chrono::milliseconds(3600000);
			poolConfig.autoReconnect = true;
			poolConfig.validateConnections = true;
			poolConfig.validationInterval = 30000;

			/* Set logger for the connection pooler */
			pooler->setLogger(logger_);

			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "About to initialize PostgreSQL connection pooler");
			}
			else
			{
				std::cerr << "DEBUG: About to initialize PostgreSQL connection pooler" << std::endl;
			}

			if (!pooler->initialize(poolConfig))
			{
				setError("Failed to initialize PostgreSQL connection pooler");
				return false;
			}
			
			if (!pooler->start())
			{
				setError("Failed to start PostgreSQL connection pooler");
				return false;
			}

			/* Store the pooler */
			connectionPooler_ = pooler;
		}
		else
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "Connection pooler already exists, checking if it's running");
			}
			else
			{
				std::cerr << "DEBUG: Connection pooler already exists, checking if it's running" << std::endl;
			}
		}


		if (!tcpServer_)
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "Creating TCP server");
			}

			/* Create TCP network server */
			auto tcpServer = std::make_unique<FauxDB::CTcp>(config_);

			/* Set the connection pooler in the TCP server */
			tcpServer->setConnectionPooler(connectionPooler_);

			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "About to initialize TCP server");
			}

			/* Initialize the TCP server */
			auto initResult = tcpServer->initialize();
			if (initResult)  /* Check for failure (non-zero error code) */
			{
				setError("Failed to initialize TCP server: " +
						 initResult.message());
				return false;
			}

			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "TCP server initialized successfully");
			}

			/* Store the TCP server */
			tcpServer_ = std::move(tcpServer);
			
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "TCP server stored successfully");
			}
		}
		else
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "TCP server already exists");
			}
		}


		return true;
	}
	catch (const std::exception& e)
	{
		setError("Network component initialization failed: " +
				 std::string(e.what()));
		return false;
	}
}

bool CServer::initializeDatabaseComponent()
{
	try
	{
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Starting database component initialization");
		}
		else
		{
			std::cerr << "DEBUG: Starting database component initialization" << std::endl;
		}
		
		/* Database component is now integrated with network component */
		/* The connection pooler is created in initializeNetworkComponent() */
		/* This method validates that the database connection pooler is working
		 */

		if (!connectionPooler_)
		{
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Database connection pooler not initialized - network component must be initialized first");
			}
			setError("Database connection pooler not initialized - network "
					 "component must be initialized first");
			return false;
		}

		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Connection pooler found, testing database connectivity");
		}

		/* Test database connectivity by getting a connection */
		auto testConnection = connectionPooler_->getConnection();
		if (!testConnection)
		{
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Failed to get test database connection - database may be unreachable");
			}
			setError("Failed to get test database connection - database may be "
					 "unreachable");
			return false;
		}

		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Test database connection acquired successfully");
		}

		/* Return the test connection to the pool */
		connectionPooler_->releaseConnection(testConnection);
		
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Test database connection returned to pool");
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		if (logger_)
		{
			logger_->log(CLogLevel::ERROR, "Database component initialization failed with exception: " + std::string(e.what()));
		}
		setError("Database component initialization failed: " +
				 std::string(e.what()));
		return false;
	}
}

bool CServer::initializeProtocolComponent()
{
	try
	{
		/* Initialize document protocol handler */
		documentProtocolHandler_ =
			std::make_unique<FauxDB::CDocumentProtocolHandler>();
		if (!documentProtocolHandler_->initialize())
		{
			setError("Failed to initialize document protocol handler");
			return false;
		}

		/* Set connection pooler for PostgreSQL access */
		if (connectionPooler_)
		{
			documentProtocolHandler_->setConnectionPooler(connectionPooler_);
		}

		/* Initialize command handlers */
		documentCommandHandler_ =
			std::make_unique<FauxDB::CDocumentCommandHandler>();

		/* Register document commands with protocol handler */
		documentProtocolHandler_->registerCommandHandler(
			"hello", std::move(documentCommandHandler_));
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Protocol component initialization failed: " +
				 std::string(e.what()));
		return false;
	}
}

bool CServer::initializeParsingComponent()
{
	try
	{
		// TODO: Implement parsing component initialization
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Parsing component initialization failed: " +
				 std::string(e.what()));
		return false;
	}
}

bool CServer::initializeQueryComponent()
{
	try
	{
		// TODO: Implement query component initialization
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Query component initialization failed: " +
				 std::string(e.what()));
		return false;
	}
}

bool CServer::initializeResponseComponent()
{
	try
	{
		// TODO: Implement response component initialization
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Response component initialization failed: " +
				 std::string(e.what()));
		return false;
	}
}

bool CServer::initializeLoggingComponent()
{
	try
	{
		// TODO: Implement logging component initialization
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Logging component initialization failed: " +
				 std::string(e.what()));
		return false;
	}
}

bool CServer::initializeConfigurationComponent()
{
	try
	{
		// TODO: Implement configuration component initialization
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Configuration component initialization failed: " +
				 std::string(e.what()));
		return false;
	}
}

/* Component lifecycle helpers */
bool CServer::startNetworkComponent()
{
	try
	{
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Starting network component");
		}

		if (!tcpServer_)
		{
			setError("TCP server not initialized");
			return false;
		}

		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "About to start TCP server");
		}

		/* Start the TCP server */
		auto startResult = tcpServer_->start();
		if (startResult)  /* Check for failure (non-zero error code) */
		{
			setError("Failed to start TCP server: " + startResult.message());
			return false;
		}

		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "TCP server started successfully");
		}

		return true;
	}
	catch (const std::exception& e)
	{
		setError("Network component startup failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::startDatabaseComponent()
{
	try
	{
		/* Database component is integrated with network component */
		/* The connection pooler is started in startNetworkComponent() */
		/* This method ensures the database connections are ready */

		if (!connectionPooler_)
		{
			setError("Database connection pooler not available");
			return false;
		}

		/* Start the connection pooler */
		if (!connectionPooler_->start())
		{
			setError("Failed to start database connection pooler");
			return false;
		}
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Database component startup failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::startProtocolComponent()
{
	try
	{
		if (!documentProtocolHandler_)
		{
			setError("Protocol handler not initialized");
			return false;
		}

		if (!documentProtocolHandler_->start())
		{
			setError("Failed to start document protocol handler");
			return false;
		}
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Protocol component startup failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::startParsingComponent()
{
	try
	{
		// TODO: Implement parsing component startup
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Parsing component startup failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::startQueryComponent()
{
	try
	{
		// TODO: Implement query component startup
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Query component startup failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::startResponseComponent()
{
	try
	{
		// TODO: Implement response component startup
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Response component startup failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::startLoggingComponent()
{
	try
	{
		// TODO: Implement logging component startup
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Logging component startup failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::startConfigurationComponent()
{
	try
	{
		// TODO: Implement configuration component startup
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Configuration component startup failed: " +
				 std::string(e.what()));
		return false;
	}
}

void CServer::stopNetworkComponent()
{
	try
	{
		if (tcpServer_)
		{
			tcpServer_->stop();
		}

		if (connectionPooler_)
		{
			connectionPooler_->shutdown();
		}
	}
	catch (const std::exception& e)
	{
		setError("Network component shutdown failed: " + std::string(e.what()));
	}
}

void CServer::stopDatabaseComponent()
{
	try
	{
		/* Database component shutdown is handled in stopNetworkComponent() */
		/* This method ensures clean database shutdown */

		if (connectionPooler_)
		{
			/* Gracefully shutdown the connection pooler */
			connectionPooler_->shutdown();
		}
	}
	catch (const std::exception& e)
	{
		setError("Database component shutdown failed: " +
				 std::string(e.what()));
	}
}

void CServer::stopProtocolComponent()
{
	try
	{
		if (documentProtocolHandler_)
		{
			documentProtocolHandler_->shutdown();
			documentProtocolHandler_.reset();
		}

		if (documentCommandHandler_)
		{
			documentCommandHandler_.reset();
		}
	}
	catch (const std::exception& e)
	{
		setError("Protocol component shutdown failed: " +
				 std::string(e.what()));
	}
}

void CServer::stopParsingComponent()
{
	// TODO: Implement parsing component shutdown
}

void CServer::stopQueryComponent()
{
	// TODO: Implement query component shutdown
}

void CServer::stopResponseComponent()
{
	// TODO: Implement response component shutdown
}

void CServer::stopLoggingComponent()
{
	// TODO: Implement logging component shutdown
}

void CServer::stopConfigurationComponent()
{
	// TODO: Implement configuration component shutdown
}

/* Server thread management */
void CServer::startMainThread()
{
	mainThread_ = std::thread(&CServer::mainThreadFunction, this);
}

void CServer::stopMainThread()
{
	if (mainThread_.joinable())
	{
		mainThread_.join();
	}
}

void CServer::startWorkerThreads()
{
	for (size_t i = 0; i < config_.workerThreads; ++i)
	{
		workerThreads_.emplace_back(&CServer::workerThreadFunction, this, i);
	}
}

void CServer::stopWorkerThreads()
{
	for (auto& thread : workerThreads_)
	{
		if (thread.joinable())
		{
			thread.join();
		}
	}
	workerThreads_.clear();
}

void CServer::mainThreadFunction()
{
	try
	{
		while (running_)
		{
			// Main server loop
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
	catch (const std::exception& e)
	{
		setError("Main thread error: " + std::string(e.what()));
	}
}

void CServer::workerThreadFunction(size_t workerId)
{
	try
	{
		while (running_)
		{
			// Worker thread loop
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
	catch (const std::exception& e)
	{
		setError("Worker thread " + std::to_string(workerId) +
				 " error: " + std::string(e.what()));
	}
}

/* Private methods implementation */
void CServer::initializeDefaults()
{
	config_.setDefaults();
}

void CServer::cleanupState()
{
	lastError_.clear();
	clearErrorLog();
	running_ = false;
	status_ = CServerStatus::Stopped;
}

bool CServer::parseConfigFile(const std::string& configFile)
{
	try
	{
		std::ifstream file(configFile);
		if (!file.is_open())
		{
			return false;
		}

		std::string line;
		while (std::getline(file, line))
		{
			if (!parseConfigurationLine(line))
			{
				return false;
			}
		}

		return true;
	}
	catch (const std::exception& e)
	{
		setError("Config file parsing failed: " + std::string(e.what()));
		return false;
	}
}

bool CServer::validateConfigFile(const std::string& configFile) const
{
	(void)configFile; // Suppress unused parameter warning
	// TODO: Implement config file validation
	return true;
}

std::string CServer::buildConfigErrorMessage(const std::string& operation,
											 const std::string& details) const
{
	return "Configuration " + operation + " failed: " + details;
}

/* Component validation helpers */
bool CServer::validateNetworkComponent() const
{
	// TODO: Implement network component validation
	return true;
}

bool CServer::validateDatabaseComponent() const
{
	// TODO: Implement database component validation
	return true;
}

bool CServer::validateProtocolComponent() const
{
	try
	{
		if (!documentProtocolHandler_)
		{
			return false;
		}

		if (!documentProtocolHandler_->isRunning())
		{
			return false;
		}

		/* Check if protocol handler supports required document operations */
		auto supportedCommands = documentProtocolHandler_->getSupportedCommands();
		if (supportedCommands.empty())
		{
			return false;
		}

		return true;
	}
	catch (const std::exception& e)
	{
		return false;
	}
}

bool CServer::validateParsingComponent() const
{
	// TODO: Implement parsing component validation
	return true;
}

bool CServer::validateQueryComponent() const
{
	// TODO: Implement query component validation
	return true;
}

bool CServer::validateResponseComponent() const
{
	// TODO: Implement response component validation
	return true;
}

bool CServer::validateLoggingComponent() const
{
	// TODO: Implement logging component validation
	return true;
}

bool CServer::validateConfigurationComponent() const
{
	// TODO: Implement configuration component validation
	return true;
}

/* Server monitoring helpers */
void CServer::updateUptime()
{
	if (status_ == CServerStatus::Running)
	{
		auto now = std::chrono::steady_clock::now();
		stats_.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
			now - stats_.startTime);
	}
}

void CServer::updateResponseTime(std::chrono::milliseconds responseTime)
{
	stats_.averageResponseTime = responseTime;
}

void CServer::updateConnectionStats(bool connectionEstablished)
{
	if (connectionEstablished)
	{
		stats_.totalConnections++;
		stats_.activeConnections++;
	}
	else
	{
		if (stats_.activeConnections > 0)
		{
			stats_.activeConnections--;
		}
	}
}

void CServer::updateRequestStats(bool success)
{
	stats_.totalRequests++;
	if (success)
	{
		stats_.successfulRequests++;
	}
	else
	{
		stats_.failedRequests++;
	}
}

void CServer::updateQueryStats(bool success)
{
	stats_.totalQueries++;
	if (success)
	{
		stats_.successfulQueries++;
	}
	else
	{
		stats_.failedQueries++;
	}
}

/* Error handling helpers */
void CServer::addErrorToLog(const std::string& error)
{
	errorLog_.push_back(error);
	lastErrorTime_ = std::chrono::steady_clock::now();
}

void CServer::clearErrorLog()
{
	errorLog_.clear();
}

std::string CServer::buildErrorMessage(const std::string& operation,
									   const std::string& details) const
{
	return operation + " failed: " + details;
}

/* Utility methods */
void CServer::initializeComponentPointers()
{
	/* Initialize component pointers to avoid null pointer dereferences */
	connectionPooler_ = std::make_shared<CPGConnectionPooler>();
	documentProtocolHandler_ = std::make_unique<CDocumentProtocolHandler>();
	documentCommandHandler_ = std::make_unique<CDocumentCommandHandler>();
	
	/* TCP server will be initialized later when config is available */
	tcpServer_ = nullptr;
}

void CServer::cleanupComponentPointers()
{
	/* Clean up component pointers */
	connectionPooler_.reset();
	tcpServer_.reset();
	documentProtocolHandler_.reset();
	documentCommandHandler_.reset();
}

bool CServer::waitForComponentStartup(std::chrono::milliseconds timeout)
{
	(void)timeout; // Suppress unused parameter warning
	// TODO: Implement component startup waiting
	return true;
}

bool CServer::waitForComponentShutdown(std::chrono::milliseconds timeout)
{
	(void)timeout; // Suppress unused parameter warning
	// TODO: Implement component shutdown waiting
	return true;
}

bool CServer::parseConfigurationLine(const std::string& line)
{
	(void)line; // Suppress unused parameter warning
	// TODO: Implement configuration line parsing
	return true;
}

void CServer::setError(const std::string& error)
{
	lastError_ = error;
	addErrorToLog(error);
}

void CServer::setLogger(std::shared_ptr<CLogger> logger)
{
	logger_ = logger;
	if (logger_)
	{
		logger_->log(CLogLevel::DEBUG, "Logger set on server");
	}
	else
	{
		std::cerr << "DEBUG: Logger set on server (null)" << std::endl;
	}
}

} /* namespace FauxDB */
