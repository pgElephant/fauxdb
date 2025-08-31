

#include <iostream>
#include <string>




#include "CConfig.hpp"
#include "CLogger.hpp"
#include "CServer.hpp"
#include "CSignal.hpp"

using namespace FauxDB;

int main(int argc, char* argv[])
{
	std::string configFile;
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if ((arg == "-c" || arg == "--config") && i + 1 < argc)
		{
			configFile = argv[i + 1];
			++i;
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

			if (auto port = loader.get("port"))
			{
				try {
					if (std::holds_alternative<std::string>(*port))
						config.port = std::stoi(std::get<std::string>(*port));
					else if (std::holds_alternative<int>(*port))
						config.port = std::get<int>(*port);
				} catch (...) {

				}
			}
			if (auto addr = loader.get("bind_address"))
			{
				if (std::holds_alternative<std::string>(*addr))
					config.bindAddress = std::get<std::string>(*addr);
			}
			if (auto threads = loader.get("worker_threads"))
			{
				try {
					if (std::holds_alternative<std::string>(*threads))
						config.workerThreads = std::stoi(std::get<std::string>(*threads));
					else if (std::holds_alternative<int>(*threads))
						config.workerThreads = std::get<int>(*threads);
				} catch (...) {

				}
			}
			if (auto loglevel = loader.get("log_level"))
			{
				if (std::holds_alternative<std::string>(*loglevel))
					config.logLevel = std::get<std::string>(*loglevel);
			}


			if (auto pgHost = loader.get("pg_host"))
			{
				if (std::holds_alternative<std::string>(*pgHost))
					config.pgHost = std::get<std::string>(*pgHost);
			}
			if (auto pgPort = loader.get("pg_port"))
			{
				try {
					if (std::holds_alternative<std::string>(*pgPort))
						config.pgPort = std::get<std::string>(*pgPort);
					else if (std::holds_alternative<int>(*pgPort))
						config.pgPort = std::to_string(std::get<int>(*pgPort));
				} catch (...) {

				}
			}
			if (auto pgDatabase = loader.get("pg_database"))
			{
				if (std::holds_alternative<std::string>(*pgDatabase))
					config.pgDatabase = std::get<std::string>(*pgDatabase);
			}
			if (auto pgUser = loader.get("pg_user"))
			{
				if (std::holds_alternative<std::string>(*pgUser))
					config.pgUser = std::get<std::string>(*pgUser);
			}
			if (auto pgPassword = loader.get("pg_password"))
			{
				if (std::holds_alternative<std::string>(*pgPassword))
					config.pgPassword = std::get<std::string>(*pgPassword);
			}
			if (auto pgPoolSize = loader.get("pg_pool_size"))
			{
				try {
					if (std::holds_alternative<std::string>(*pgPoolSize))
						config.pgPoolSize = std::stoul(std::get<std::string>(*pgPoolSize));
					else if (std::holds_alternative<int>(*pgPoolSize))
						config.pgPoolSize = std::get<int>(*pgPoolSize);
				} catch (...) {

				}
			}
			if (auto pgTimeout = loader.get("pg_timeout"))
			{
				try {
					if (std::holds_alternative<std::string>(*pgTimeout))
						config.pgTimeout = std::chrono::milliseconds(std::stoul(std::get<std::string>(*pgTimeout)) * 1000);
					else if (std::holds_alternative<int>(*pgTimeout))
						config.pgTimeout = std::chrono::milliseconds(std::get<int>(*pgTimeout) * 1000);
				} catch (...) {

				}
			}
		}
		else
		{
			config.setDefaults();
		}



		CLogger logger(config);
		logger.enableConsoleOutput(true);
		logger.setLogLevel(CLogLevel::INFO);
		std::string logFile = configFile.empty() ? "fauxdb.log" : configFile;
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
