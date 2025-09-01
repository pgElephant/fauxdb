

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>




#include "CPGConnectionPooler.hpp"
#include "CDatabase.hpp"
#include "CPostgresDatabase.hpp"

using namespace std;
namespace FauxDB
{


CPGConnectionPooler::CPGConnectionPooler()
	: host_("localhost"), port_("5432"), databaseName_("fauxdb"),
	  username_("postgres"), password_("")
{

}


CPGConnectionPooler::~CPGConnectionPooler()
{
	shutdown();
}


shared_ptr<void> CPGConnectionPooler::getConnection()
{
	shared_ptr<PGConnection> pgConn = getPostgresConnection();
	if (pgConn)
		return static_pointer_cast<void>(pgConn);
	return nullptr;
}


void CPGConnectionPooler::releaseConnection(shared_ptr<void> connection)
{
	if (connection)
	{
		shared_ptr<PGConnection> pgConn =
			static_pointer_cast<PGConnection>(connection);
		releasePostgresConnection(pgConn);
	}
}


bool CPGConnectionPooler::returnConnection(shared_ptr<void> connection)
{
	if (connection)
	{
		shared_ptr<PGConnection> pgConn =
			static_pointer_cast<PGConnection>(connection);
		releasePostgresConnection(pgConn);
		return true;
	}
	return false;
}


bool CPGConnectionPooler::initialize(const CConnectionPoolConfig& config)
{
	lock_guard<mutex> lock(poolMutex_);
	try
	{
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Initializing PostgreSQL connection pool with config: max_connections=" + 
				std::to_string(config.maxConnections) + ", min_connections=" + std::to_string(config.minConnections) + 
				", initial_connections=" + std::to_string(config.initialConnections));
		}
		else
		{
			std::cerr << "DEBUG: Initializing PostgreSQL connection pool with config: max_connections=" << config.maxConnections 
					  << ", min_connections=" << config.minConnections 
					  << ", initial_connections=" << config.initialConnections << std::endl;
		}
		
		config_ = config;
		for (size_t i = 0; i < config_.initialConnections; ++i)
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "Creating initial connection " + std::to_string(i + 1) + "/" + std::to_string(config_.initialConnections));
			}
			
			if (!addConnection())
			{
				logConnectionEvent("INITIALIZATION_FAILED",
								   "Failed to create initial connection " +
									   to_string(i));
				return false;
			}
		}
		logConnectionEvent("INITIALIZATION_SUCCESS",
						   "Pool initialized with " +
							   to_string(config_.initialConnections) +
							   " connections");
		return true;
	}
	catch (const exception& e)
	{
		logConnectionEvent("INITIALIZATION_ERROR",
						   "Exception during initialization: " +
							   string(e.what()));
		return false;
	}
}


bool CPGConnectionPooler::start()
{
	lock_guard<mutex> lock(poolMutex_);
	if (isRunning_)
		return true;
	try
	{
		isRunning_ = true;



		logConnectionEvent("POOL_STARTED",
						   "Connection pool started successfully");
		return true;
	}
	catch (const exception& e)
	{
		logConnectionEvent("POOL_START_ERROR",
						   "Exception during pool start: " + string(e.what()));
		isRunning_ = false;
		return false;
	}
}


void CPGConnectionPooler::stop()
{
	lock_guard<mutex> lock(poolMutex_);
	if (!isRunning_)
		return;
	isRunning_ = false;
	connectionAvailable_.notify_all();
	logConnectionEvent("POOL_STOPPED", "Connection pool stopped");
}


bool CPGConnectionPooler::isRunning() const
{
	lock_guard<mutex> lock(poolMutex_);
	return isRunning_;
}


void CPGConnectionPooler::shutdown()
{
	lock_guard<mutex> lock(poolMutex_);
	isRunning_ = false;
	for (auto& conn : connections_)
	{
		if (conn && conn->database)
			conn->database->disconnect();
	}
	connections_.clear();
	availableConnections_.clear();
	inUseConnections_.clear();
	logConnectionEvent("POOL_SHUTDOWN", "Connection pool shutdown complete");
}

bool CPGConnectionPooler::addConnection()
{
	try
	{
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Creating new PostgreSQL connection. Current pool size: " + 
				std::to_string(connections_.size()) + "/" + std::to_string(config_.maxConnections));
		}
		
		shared_ptr<PGConnection> newConn = createNewConnection();
		if (newConn)
		{
			connections_.push_back(newConn);
			availableConnections_.push_back(newConn);
			logConnectionEvent("CONNECTION_ADDED",
							   "New connection added to pool");
			return true;
		}
		return false;
	}
	catch (const exception& e)
	{
		logConnectionEvent("CONNECTION_ADD_ERROR",
						   "Exception adding connection: " + string(e.what()));
		return false;
	}
}

bool CPGConnectionPooler::removeConnection(shared_ptr<void> connection)
{
	if (!connection)
		return false;
	shared_ptr<PGConnection> pgConn =
		static_pointer_cast<PGConnection>(connection);
	try
	{
		auto removeFromVector = [&pgConn](vector<shared_ptr<PGConnection>>& vec)
		{ vec.erase(remove(vec.begin(), vec.end(), pgConn), vec.end()); };
		removeFromVector(connections_);
		removeFromVector(availableConnections_);
		removeFromVector(inUseConnections_);
		closeConnection(pgConn);
		logConnectionEvent("CONNECTION_REMOVED",
						   "Connection removed from pool");
		return true;
	}
	catch (const exception& e)
	{
		logConnectionEvent("CONNECTION_REMOVE_ERROR",
						   "Exception removing connection: " +
							   string(e.what()));
		return false;
	}
}

void CPGConnectionPooler::setPostgresConfig(const string& host,
											const string& port,
											const string& database,
											const string& username,
											const string& password)
{
	lock_guard<mutex> lock(poolMutex_);
	host_ = host;
	port_ = port;
	databaseName_ = database;
	username_ = username;
	password_ = password;
	
	if (logger_)
	{
		logger_->log(CLogLevel::DEBUG, "PostgreSQL config set: host=" + host + ", port=" + port + 
			", database=" + database + ", user=" + username);
	}
	else
	{
		std::cerr << "DEBUG: PostgreSQL config set: host=" << host << ", port=" << port 
				  << ", database=" << database << ", user=" << username << std::endl;
	}
	
	logConnectionEvent("CONFIG_UPDATED", "PostgreSQL configuration updated");
}

shared_ptr<PGConnection> CPGConnectionPooler::getPostgresConnection()
{
	unique_lock<mutex> lock(poolMutex_);
	if (!isRunning_)
		return nullptr;
	
	if (logger_)
	{
		logger_->log(CLogLevel::DEBUG, "Requesting PostgreSQL connection from pool. Available: " + 
			std::to_string(availableConnections_.size()) + ", Total: " + std::to_string(connections_.size()));
	}
	
	while (availableConnections_.empty())
	{
		if (connections_.size() < config_.maxConnections)
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "No available connections, creating new connection. Current: " + 
					std::to_string(connections_.size()) + "/" + std::to_string(config_.maxConnections));
			}
			
			// Create connection while holding the lock to prevent race conditions
			shared_ptr<PGConnection> newConn = createNewConnection();
			if (newConn)
			{
				connections_.push_back(newConn);
				availableConnections_.push_back(newConn);
				logConnectionEvent("CONNECTION_ADDED", "New connection added to pool");
				continue; // Loop will now find the available connection
			}
			else
			{
				break; // Failed to create connection
			}
		}
		if (config_.connectionTimeout.count() > 0)
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "Waiting for available connection with timeout: " + 
					std::to_string(config_.connectionTimeout.count()) + "ms");
			}
			
			auto timeout =
				chrono::steady_clock::now() + config_.connectionTimeout;
			if (connectionAvailable_.wait_until(lock, timeout) ==
				cv_status::timeout)
			{
				logConnectionEvent("CONNECTION_TIMEOUT",
								   "Timeout waiting for available connection");
				return nullptr;
			}
		}
		else
		{
			connectionAvailable_.wait(lock);
		}
	}
	
	if (!availableConnections_.empty())
	{
		shared_ptr<PGConnection> conn = availableConnections_.back();
		if (validateConnection(conn))
		{
			availableConnections_.pop_back();  // Remove AFTER validation but BEFORE markConnectionInUse
			inUseConnections_.push_back(conn); // Directly add to inUse list
			logConnectionEvent("CONNECTION_ACQUIRED",
							   "Connection acquired from pool. Ptr: " + std::to_string(reinterpret_cast<uintptr_t>(conn.get())));
			return conn;
		}
		else
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "Connection validation failed, removing broken connection and retrying");
			}
			
			removeBrokenConnection(conn);
			lock.unlock();
			return getPostgresConnection();
		}
	}
	return nullptr;
}

void CPGConnectionPooler::releasePostgresConnection(std::shared_ptr<PGConnection> connection)
{
	if (!connection)
	{
		return;
	}

	try
	{
		// Mark connection as available
		markConnectionAvailable(connection);
		
		// Notify waiting threads that a connection is available
		connectionAvailable_.notify_one();
	}
	catch (const std::exception& e)
	{
		// Log error but don't fail
		(void)e;
	}
}

void CPGConnectionPooler::resetStats()
{
	std::lock_guard<std::mutex> lock(statsMutex_);
	stats_ = CConnectionPoolStats{};
}

bool CPGConnectionPooler::healthCheck()
{
	try
	{
		// Try to get a connection to test health
		auto connection = getConnection();
		if (connection)
		{
			returnConnection(connection);
			return true;
		}
		return false;
	}
	catch (const std::exception& e)
	{
		(void)e;
		return false;
	}
}

void CPGConnectionPooler::setMaxConnections(size_t maxConnections)
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	maxConnections_ = maxConnections;
}

void CPGConnectionPooler::setMinConnections(size_t minConnections)
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	// Note: minConnections_ member variable doesn't exist in this class
	(void)minConnections;
}

void CPGConnectionPooler::setConnectionFactory(std::function<std::shared_ptr<void>()> factory)
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	connectionFactory_ = factory;
}

void CPGConnectionPooler::setConnectionTimeout(std::chrono::milliseconds timeout)
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	connectionTimeout_ = timeout;
}

void CPGConnectionPooler::setConnectionValidator(std::function<bool(std::shared_ptr<void>)> validator)
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	connectionValidator_ = validator;
}

void CPGConnectionPooler::markConnectionAvailable(std::shared_ptr<PGConnection> connection)
{
	if (!connection)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(poolMutex_);
	
	// Move connection from inUseConnections_ to availableConnections_
	// Use raw pointer comparison to handle shared_ptr cast issues
	auto it = std::find_if(inUseConnections_.begin(), inUseConnections_.end(),
		[&connection](const std::shared_ptr<PGConnection>& conn) {
			return conn.get() == connection.get();
		});
	if (it != inUseConnections_.end())
	{
		inUseConnections_.erase(it);
		availableConnections_.push_back(connection);
		connection->lastUsed = std::chrono::steady_clock::now();
		
		logConnectionEvent("CONNECTION_RELEASED", 
						   "Connection returned to pool. Ptr: " + std::to_string(reinterpret_cast<uintptr_t>(connection.get())));
	}
	else
	{
		logConnectionEvent("CONNECTION_ERROR", 
						   "Attempted to release connection not in use. Ptr: " + std::to_string(reinterpret_cast<uintptr_t>(connection.get())) + 
						   ", InUse count: " + std::to_string(inUseConnections_.size()));
	}
}

void CPGConnectionPooler::setConnectionFailedCallback(std::function<void(std::shared_ptr<void>, const std::string&)> callback)
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	connectionFailedCallback_ = callback;
}

void CPGConnectionPooler::setConnectionAcquiredCallback(std::function<void(std::shared_ptr<void>)> callback)
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	connectionAcquiredCallback_ = callback;
}

void CPGConnectionPooler::setConnectionReleasedCallback(std::function<void(std::shared_ptr<void>)> callback)
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	connectionReleasedCallback_ = callback;
}

void CPGConnectionPooler::setConfig(const CConnectionPoolConfig& config)
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	config_ = config;
	maxConnections_ = config.maxConnections;
	// Note: minConnections_ member variable doesn't exist in this class
	connectionTimeout_ = config.connectionTimeout;
}

std::string CPGConnectionPooler::getStatusReport() const
{
	std::lock_guard<std::mutex> lock(statsMutex_);
	
	std::stringstream ss;
	ss << "Connection Pool Status:\n";
	ss << "  Total Connections: " << stats_.totalConnections << "\n";
	ss << "  Available Connections: " << stats_.availableConnections << "\n";
	ss << "  In Use Connections: " << stats_.inUseConnections << "\n";
	ss << "  Broken Connections: " << stats_.brokenConnections << "\n";
	ss << "  Average Response Time: " << stats_.averageResponseTime.count() << "ms\n";
	
	return ss.str();
}

size_t CPGConnectionPooler::getInUseConnections() const
{
	std::lock_guard<std::mutex> lock(statsMutex_);
	return stats_.inUseConnections;
}

size_t CPGConnectionPooler::getAvailableConnections() const
{
	std::lock_guard<std::mutex> lock(statsMutex_);
	return stats_.availableConnections;
}

CConnectionPoolStats CPGConnectionPooler::getStats() const
{
	std::lock_guard<std::mutex> lock(statsMutex_);
	return stats_;
}

CConnectionPoolConfig CPGConnectionPooler::getConfig() const
{
	std::lock_guard<std::mutex> lock(poolMutex_);
	return config_;
}

size_t CPGConnectionPooler::getActiveConnections() const
{
	lock_guard<mutex> lock(poolMutex_);
	return inUseConnections_.size();
}

size_t CPGConnectionPooler::getIdleConnections() const
{
	lock_guard<mutex> lock(poolMutex_);
	return availableConnections_.size();
}

size_t CPGConnectionPooler::getTotalConnections() const
{
	lock_guard<mutex> lock(poolMutex_);
	return connections_.size();
}

void CPGConnectionPooler::logConnectionEvent(const string& event,
											 const string& details)
{
	CConnectionPooler::logConnectionEvent(event, details);
	if (event == "CONNECTION_ACQUIRED")
		stats_.successfulRequests++;
	else if (event == "CONNECTION_TIMEOUT" || event == "INITIALIZATION_FAILED")
		stats_.failedRequests++;
	
	/* Only show important events */
	if (event == "INITIALIZATION_FAILED" || event == "INITIALIZATION_ERROR" || 
		event == "CONNECTION_CREATE_ERROR" || event == "CONNECTION_TIMEOUT") {
		if (logger_)
		{
			logger_->log(CLogLevel::ERROR, event + ": " + details);
		}
		else
		{
			// Fallback to base class implementation
			CConnectionPooler::logConnectionEvent(event, details);
		}
	}
}

void CPGConnectionPooler::performMaintenance()
{

	cleanupBrokenConnections();
	validateConnections();
	adjustPoolSize();
	cleanupExpiredConnections();


}

void CPGConnectionPooler::cleanupBrokenConnections()
{
	lock_guard<mutex> lock(poolMutex_);
	auto it = connections_.begin();
	while (it != connections_.end())
	{
		if (*it && !validateConnection(*it))
		{
			logConnectionEvent("BROKEN_CONNECTION_REMOVED",
							   "Removing broken connection");
			closeConnection(*it);
			it = connections_.erase(it);
		}
		else
			++it;
	}
	availableConnections_.erase(
		remove_if(availableConnections_.begin(), availableConnections_.end(),
				  [this](const shared_ptr<PGConnection>& conn)
				  { return !conn || !validateConnection(conn); }),
		availableConnections_.end());
	inUseConnections_.erase(
		remove_if(inUseConnections_.begin(), inUseConnections_.end(),
				  [this](const shared_ptr<PGConnection>& conn)
				  { return !conn || !validateConnection(conn); }),
		inUseConnections_.end());
}

void CPGConnectionPooler::validateConnections()
{

	lock_guard<mutex> lock(poolMutex_);
	for (auto& conn : availableConnections_)
	{
		if (conn && !validateConnection(conn))
		{
			logConnectionEvent("CONNECTION_VALIDATION_FAILED",
							   "Connection failed validation");
			removeBrokenConnection(conn);
		}
	}

}

void CPGConnectionPooler::adjustPoolSize()
{
	lock_guard<mutex> lock(poolMutex_);
	size_t currentSize = connections_.size();
	size_t targetSize = currentSize;
	if (availableConnections_.size() > config_.minConnections &&
		availableConnections_.size() > inUseConnections_.size())
	{
		targetSize = max(config_.minConnections, availableConnections_.size() -
													 inUseConnections_.size());
	}
	if (currentSize < config_.minConnections)
		targetSize = config_.minConnections;
	while (connections_.size() < targetSize)
	{
		if (!addConnection())
			break;
	}
	while (connections_.size() > targetSize &&
		   connections_.size() > config_.minConnections)
	{
		if (!availableConnections_.empty())
		{
			shared_ptr<PGConnection> conn = availableConnections_.back();
			availableConnections_.pop_back();
			removeConnection(static_pointer_cast<void>(conn));
		}
		else
			break;
	}
}

shared_ptr<PGConnection> CPGConnectionPooler::createNewConnection()
{
	try
	{
		auto db = make_shared<CPostgresDatabase>();
		CDatabaseConfig config;
		config.host = host_;
		config.port = port_;
		config.database = databaseName_;
		config.username = username_;
		config.password = password_;
		config.connectionTimeout = chrono::milliseconds(5000);
		config.queryTimeout = chrono::milliseconds(30000);
		config.maxConnections = 1;
		config.autoCommit = true;
		config.sslEnabled = false;		
		// Debug connection creation
		
		db->setConfig(config);
		if (logger_)
		{
			logger_->log(CLogLevel::DEBUG, "Attempting to connect to PostgreSQL with config: host=" + config.host + 
				", port=" + config.port + ", database=" + config.database + ", user=" + config.username);
		}
		
		if (db->connect(config))
		{
			if (logger_)
			{
				logger_->log(CLogLevel::DEBUG, "PostgreSQL connection created successfully");
			}
			logConnectionEvent(
				"CONNECTION_CREATED",
				"New PostgreSQL connection created successfully");
			return make_shared<PGConnection>(db);
		}
		else
		{
			string error = db->getLastError();
			if (logger_)
			{
				logger_->log(CLogLevel::ERROR, "Failed to create PostgreSQL connection: " + error);
			}
			logConnectionEvent("CONNECTION_CREATE_ERROR",
							   "Failed to create connection: " + error);
			return nullptr;
		}
	}
	catch (const exception& e)
	{
		logConnectionEvent("CONNECTION_CREATE_EXCEPTION",
						   "Exception creating connection: " +
							   string(e.what()));
		return nullptr;
	}
}

bool CPGConnectionPooler::validateConnection(shared_ptr<void> connection)
{
	if (!connection)
		return false;
	auto pgConnection = static_pointer_cast<PGConnection>(connection);
	if (!pgConnection || !pgConnection->database)
		return false;
	try
	{
		if (pgConnection->database->isConnected())
			return true;
		return false;
	}
	catch (...)
	{
		return false;
	}
}

void CPGConnectionPooler::closeConnection(shared_ptr<PGConnection> connection)
{
	if (connection && connection->database)
		connection->database->disconnect();
}

void CPGConnectionPooler::markConnectionInUse(
	shared_ptr<PGConnection> connection)
{
	if (connection)
	{
		/* Caller already holds poolMutex_, so no need to lock again */
		auto it = find(availableConnections_.begin(),
					   availableConnections_.end(), connection);
		if (it != availableConnections_.end())
		{
			availableConnections_.erase(it);
			inUseConnections_.push_back(connection);
		}
	}
}

void CPGConnectionPooler::removeBrokenConnection(
	shared_ptr<PGConnection> connection)
{
	if (connection)
	{
		/* Caller already holds poolMutex_, so no need to lock again */

		auto it1 = find(availableConnections_.begin(),
						availableConnections_.end(), connection);
		if (it1 != availableConnections_.end())
		{
			availableConnections_.erase(it1);
		}

		auto it2 = find(inUseConnections_.begin(), inUseConnections_.end(),
						connection);
		if (it2 != inUseConnections_.end())
		{
			inUseConnections_.erase(it2);
		}

		if (connection->database)
		{
			connection->database->disconnect();
		}
	}
}

void CPGConnectionPooler::cleanupExpiredConnections()
{
	lock_guard<mutex> lock(poolMutex_);
	auto now = chrono::steady_clock::now();


	auto it = availableConnections_.begin();
	while (it != availableConnections_.end())
	{
		if (now - (*it)->lastUsed > connectionTimeout_)
		{
			if ((*it)->database)
			{
				(*it)->database->disconnect();
			}
			it = availableConnections_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

} // namespace FauxDB

