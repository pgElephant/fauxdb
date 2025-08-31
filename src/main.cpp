

#include <iostream>
#include <string>




#include "CConfig.hpp"
#include "CLogger.hpp"
#include "CServer.hpp"
#include "CSignal.hpp"

// Helper function to get nested configuration values
template<typename T>
T getConfigValue(const FauxDB::CConfig& loader, const std::string& key, const T& defaultValue) {
    // Try direct key first
    if (auto value = loader.get(key)) {
        if (std::holds_alternative<T>(*value)) {
            return std::get<T>(*value);
        } else if (std::holds_alternative<std::string>(*value)) {
            try {
                if constexpr (std::is_same_v<T, int>) {
                    return std::stoi(std::get<std::string>(*value));
                } else if constexpr (std::is_same_v<T, uint64_t>) {
                    return std::stoull(std::get<std::string>(*value));
                }
            } catch (...) {
                return defaultValue;
            }
        }
    }
    
    // Try nested keys for JSON/YAML format
    std::vector<std::string> nestedKeys = {
        "server." + key,
        "postgresql." + key,
        "logging." + key,
        "security." + key,
        "performance." + key
    };
    
    for (const auto& nestedKey : nestedKeys) {
        if (auto value = loader.get(nestedKey)) {
            if (std::holds_alternative<T>(*value)) {
                return std::get<T>(*value);
            } else if (std::holds_alternative<std::string>(*value)) {
                try {
                    if constexpr (std::is_same_v<T, int>) {
                        return std::stoi(std::get<std::string>(*value));
                    } else if constexpr (std::is_same_v<T, uint64_t>) {
                        return std::stoull(std::get<std::string>(*value));
                    }
                } catch (...) {
                    continue;
                }
            }
        }
    }
    
    return defaultValue;
}

std::string getConfigString(const FauxDB::CConfig& loader, const std::string& key, const std::string& defaultValue) {
    // Try direct key first
    if (auto value = loader.get(key)) {
        if (std::holds_alternative<std::string>(*value)) {
            return std::get<std::string>(*value);
        }
    }
    
    // Try nested keys for JSON/YAML format
    std::vector<std::string> nestedKeys = {
        "server." + key,
        "postgresql." + key,
        "logging." + key,
        "security." + key,
        "performance." + key
    };
    
    for (const auto& nestedKey : nestedKeys) {
        if (auto value = loader.get(nestedKey)) {
            if (std::holds_alternative<std::string>(*value)) {
                return std::get<std::string>(*value);
            }
        }
    }
    
    return defaultValue;
}

using namespace FauxDB;

int main(int argc, char* argv[])
{
	std::string configFile;
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "-h" || arg == "--help")
		{
			std::cout << "FauxDB - MongoDB to PostgreSQL Query Translator\n";
			std::cout << "Usage: " << argv[0] << " [OPTIONS]\n\n";
			std::cout << "Options:\n";
			std::cout << "  -c, --config <file>    Configuration file (supports .conf, .json, .yaml, .yml)\n";
			std::cout << "  -h, --help             Show this help message\n";
			std::cout << "  -v, --version          Show version information\n\n";
			std::cout << "Configuration file formats supported:\n";
			std::cout << "  - INI/Conf files (.conf)\n";
			std::cout << "  - JSON files (.json)\n";
			std::cout << "  - YAML files (.yaml, .yml)\n";
			std::cout << "  - TOML files (.toml)\n\n";
			std::cout << "Example configuration files are available in the config/ directory.\n";
			return 0;
		}
		else if (arg == "-v" || arg == "--version")
		{
			std::cout << "FauxDB version 1.0.0\n";
			std::cout << "MongoDB to PostgreSQL Query Translator\n";
			return 0;
		}
		else if ((arg == "-c" || arg == "--config") && i + 1 < argc)
		{
			configFile = argv[i + 1];
			++i;
		}
		else
		{
			std::cerr << "Unknown option: " << arg << std::endl;
			std::cerr << "Use --help for usage information.\n";
			return 1;
		}
	}


	try
	{
		CServerConfig config;
		if (!configFile.empty())
		{

			CConfig loader;
			auto err = loader.loadFromFile(configFile);
			if (err)
			{
				std::cerr << "Failed to load config file: " << configFile
						  << ", error=" << err.message() << std::endl;
				return 1;
			}

			// Load configuration using helper functions
			config.port = getConfigValue<int>(loader, "port", 27017);
			config.bindAddress = getConfigString(loader, "bind_address", "0.0.0.0");
			config.maxConnections = getConfigValue<int>(loader, "max_connections", 1000);
			config.workerThreads = getConfigValue<int>(loader, "worker_threads", 4);
			config.logLevel = getConfigString(loader, "log_level", "INFO");
			
			// PostgreSQL configuration
			config.pgHost = getConfigString(loader, "host", "localhost");
			config.pgPort = getConfigString(loader, "port", "5432");
			config.pgDatabase = getConfigString(loader, "database", "fauxdb");
			config.pgUser = getConfigString(loader, "user", "fauxdb");
			config.pgPassword = getConfigString(loader, "password", "fauxdb");
			config.pgPoolSize = getConfigValue<uint64_t>(loader, "pool_size", 10);
			
			// Convert timeout to milliseconds
			int timeoutSeconds = getConfigValue<int>(loader, "timeout", 10);
			config.pgTimeout = std::chrono::milliseconds(timeoutSeconds * 1000);
			
			// Debug output
			std::cout << "Configuration loaded:" << std::endl;
			std::cout << "  Port: " << config.port << std::endl;
			std::cout << "  Bind Address: " << config.bindAddress << std::endl;
			std::cout << "  Worker Threads: " << config.workerThreads << std::endl;
			std::cout << "  Max Connections: " << config.maxConnections << std::endl;
			std::cout << "  PostgreSQL Host: " << config.pgHost << std::endl;
			std::cout << "  PostgreSQL Port: " << config.pgPort << std::endl;
			std::cout << "  PostgreSQL Database: " << config.pgDatabase << std::endl;
		}
		else
		{
			config.setDefaults();
		}



		CLogger logger(config);
		logger.enableConsoleOutput(true);
		logger.setLogLevel(CLogLevel::INFO);
		std::string logFile = "fauxdb.log";
		logger.setLogFile(logFile);
		logger.initialize();

		CServer server;
		if (!server.initialize(config))
		{
			logger.log(CLogLevel::ERROR,
					   "[Startup] FauxDB daemon failed to initialize server "
					   "with config: address=" +
						   config.bindAddress +
						   ", port=" + std::to_string(config.port) +
						   ", threads=" + std::to_string(config.workerThreads));
			return 1;
		}


		if (!server.start())
		{
			logger.log(
				CLogLevel::ERROR,
				"[Startup] FauxDB daemon failed to start server process. Check "
				"network/database configuration and logs for details.");
			return 1;
		}


		logger.log(
			CLogLevel::INFO,
			"[Startup] FauxDB daemon started successfully. Listening on " +
				config.bindAddress + ":" + std::to_string(config.port) +
				", worker threads=" + std::to_string(config.workerThreads));
		logger.log(CLogLevel::INFO,
				   "[Startup] Server Info: " + server.getServerInfo());
		logger.log(CLogLevel::INFO,
				   "[Startup] Database Status: " + server.getDatabaseStatus());
		logger.log(CLogLevel::INFO,
				   "[Startup] Network Status: " + server.getNetworkStatus());
		logger.log(CLogLevel::INFO,
				   "[Startup] Initial Server Statistics: " +
					   server.getServerStatistics());


		CSignal signalHandler;
		signalHandler.initialize();


		while (server.isRunning())
		{
			static auto lastStatusUpdate = chrono::steady_clock::now();
			auto now = chrono::steady_clock::now();
			if (chrono::duration_cast<chrono::seconds>(now - lastStatusUpdate)
					.count() >= 30)
			{
				logger.log(
					CLogLevel::INFO,
					"[Status] FauxDB daemon periodic update: Database "
					"Status: " +
						server.getDatabaseStatus() +
						", Network Status: " + server.getNetworkStatus());
				lastStatusUpdate = now;
			}
			this_thread::sleep_for(chrono::milliseconds(100));
		}


		server.shutdown();
		logger.log(CLogLevel::INFO,
				   "[Shutdown] FauxDB daemon shutdown complete. All resources "
				   "released and server stopped.");
		logger.shutdown();
		return 0;
	}
	catch (const exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}
}
