/*-------------------------------------------------------------------------
 *
 * main.cpp
 *		  Main entry point for FauxDB server
 *
 * Handles server initialization, configuration, and daemon mode.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * IDENTIFICATION
 *		  src/main.cpp
 *
 *-------------------------------------------------------------------------
 */

#include "CConfig.hpp"
#include "CLogger.hpp"
#include "CServer.hpp"
#include "CSignal.hpp"

#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

using namespace FauxDB;
using namespace std::chrono;

template <typename T>
T
get_config_value(const FauxDB::CConfig& loader, const std::string& key,
				 const T& defaultValue)
{
	std::vector<std::string> nestedKeys = {"server." + key, "postgresql." + key,
										   "logging." + key, "security." + key,
										   "performance." + key};

	if (auto value = loader.get(key))
	{
		if (std::holds_alternative<T>(*value))
			return std::get<T>(*value);
		else if (std::holds_alternative<std::string>(*value))
		{
			try
			{
				if constexpr (std::is_same_v<T, int>)
					return std::stoi(std::get<std::string>(*value));
				else if constexpr (std::is_same_v<T, uint64_t>)
					return std::stoull(std::get<std::string>(*value));
				else if constexpr (std::is_same_v<T, bool>)
				{
					std::string val = std::get<std::string>(*value);
					return val == "true" || val == "1" || val == "yes";
				}
			}
			catch (...)
			{
				return defaultValue;
			}
		}
	}

	for (const auto& nestedKey : nestedKeys)
	{
		if (auto value = loader.get(nestedKey))
		{
			if (std::holds_alternative<T>(*value))
				return std::get<T>(*value);
			else if (std::holds_alternative<std::string>(*value))
			{
				try
				{
					if constexpr (std::is_same_v<T, int>)
						return std::stoi(std::get<std::string>(*value));
					else if constexpr (std::is_same_v<T, uint64_t>)
						return std::stoull(std::get<std::string>(*value));
					else if constexpr (std::is_same_v<T, bool>)
					{
						std::string val = std::get<std::string>(*value);
						return val == "true" || val == "1" || val == "yes";
					}
				}
				catch (...)
				{
					continue;
				}
			}
		}
	}

	return defaultValue;
}

std::string
get_config_string(const FauxDB::CConfig& loader, const std::string& key,
				  const std::string& defaultValue)
{
	std::vector<std::string> nestedKeys = {"server." + key, "postgresql." + key,
										   "logging." + key, "security." + key,
										   "performance." + key};

	if (auto value = loader.get(key))
	{
		if (std::holds_alternative<std::string>(*value))
			return std::get<std::string>(*value);
	}

	for (const auto& nestedKey : nestedKeys)
	{
		if (auto value = loader.get(nestedKey))
		{
			if (std::holds_alternative<std::string>(*value))
				return std::get<std::string>(*value);
		}
	}

	return defaultValue;
}

static bool
daemonize(const std::string& pidFile = "/var/run/fauxdb.pid")
{
	pid_t		pid;
	FILE	   *pidFileHandle;

	pid = fork();
	if (pid < 0)
		return false;
	if (pid > 0)
		exit(0);

	if (setsid() < 0)
		return false;

	pid = fork();
	if (pid < 0)
		return false;
	if (pid > 0)
		exit(0);

	umask(0);

	pidFileHandle = fopen(pidFile.c_str(), "w");
	if (pidFileHandle)
	{
		fprintf(pidFileHandle, "%d\n", getpid());
		fclose(pidFileHandle);
	}

	chdir("/");

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	open("/dev/null", O_RDONLY);
	open("/dev/null", O_WRONLY);
	open("/dev/null", O_WRONLY);

	return true;
}

int
main(int argc, char **argv)
{
	FauxDB::CConfig				loader;
	std::string					configFile;
	bool						daemonMode = false;
	std::string					arg;
	CServerConfig				config;
	std::error_code				err;
	int							timeoutSeconds;
	char						cwd[1024];
	std::string					pidFile;
	std::string					logFile = "fauxdb.log";
	std::shared_ptr<CLogger>	loggerPtr;
	CServer						server;
	CSignal						signal;
	std::string					lastError;

	for (int i = 1; i < argc; ++i)
	{
		arg = argv[i];

		if (arg == "-h" || arg == "--help")
		{
			std::cout << "FauxDB - Document Database Engine to PostgreSQL\n";
			std::cout << "Usage: " << argv[0] << " [OPTIONS]\n\n";
			std::cout << "Options:\n";
			std::cout << "  -c, --config <file>    Configuration file "
						 "(supports .conf, .json, .yaml, .yml)\n";
			std::cout << "  -d, --daemon           Run in daemon mode (background)\n";
			std::cout << "  -h, --help             Show this help message\n";
			std::cout << "  -v, --version          Show version information\n\n";
			std::cout << "Configuration file formats supported:\n";
			std::cout << "  - INI/Conf files (.conf)\n";
			std::cout << "  - JSON files (.json)\n";
			std::cout << "  - YAML files (.yaml, .yml)\n";
			std::cout << "  - TOML files (.toml)\n\n";
			std::cout << "Example configuration files are available in the "
						 "config/ directory.\n";
			return 0;
		}
		else if (arg == "-v" || arg == "--version")
		{
			std::cout << "FauxDB version 1.0.0\n";
			std::cout << "Document Database Query Translator\n";
			return 0;
		}
		else if ((arg == "-c" || arg == "--config") && i + 1 < argc)
		{
			configFile = argv[i + 1];
			++i;
		}
		else if (arg == "-d" || arg == "--daemon")
		{
			daemonMode = true;
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
		if (!configFile.empty())
		{
			err = loader.loadFromFile(configFile);
			if (err)
			{
				std::cerr << "Failed to load config file: " << configFile
						  << ", error=" << err.message() << std::endl;
				return 1;
			}

			config.port = get_config_value<int>(loader, "port", 27017);
			config.bindAddress = get_config_string(loader, "bind_address", "0.0.0.0");
			config.maxConnections = get_config_value<int>(loader, "max_connections", 1000);
			config.workerThreads = get_config_value<int>(loader, "worker_threads", 4);
			config.logLevel = get_config_string(loader, "log_level", "INFO");

			config.pgHost = get_config_string(loader, "postgresql.host", "localhost");
			config.pgPort = get_config_string(loader, "postgresql.port", "5432");
			config.pgDatabase = get_config_string(loader, "postgresql.database", "fauxdb");
			config.pgUser = get_config_string(loader, "postgresql.user", "fauxdb");
			config.pgPassword = get_config_string(loader, "postgresql.password", "fauxdb");
			config.pgPoolSize = get_config_value<uint64_t>(loader, "postgresql.pool_size", 10);

			timeoutSeconds = get_config_value<int>(loader, "postgresql.timeout", 10);
			config.pgTimeout = std::chrono::milliseconds(timeoutSeconds * 1000);

			config.daemonMode = get_config_value<bool>(loader, "daemon_mode", false);

			config.mongodbServerAuthMethod = get_config_string(loader, "server_auth.method", "scram-sha-256");
			config.mongodbServerAuthRequired = get_config_value<bool>(loader, "server_auth.required", false);
			config.mongodbServerAuthDatabase = get_config_string(loader, "server_auth.database", "admin");
			config.mongodbServerAuthUsername = get_config_string(loader, "server_auth.username", "");
			config.mongodbServerAuthPassword = get_config_string(loader, "server_auth.password", "");
			config.mongodbServerAuthUseSSL = get_config_value<bool>(loader, "server_auth.use_ssl", false);
			config.mongodbServerAuthSSLCert = get_config_string(loader, "server_auth.ssl_cert", "");
			config.mongodbServerAuthSSLKey = get_config_string(loader, "server_auth.ssl_key", "");
			config.mongodbServerAuthSSLCA = get_config_string(loader, "server_auth.ssl_ca", "");

			config.postgresqlClientAuthMethod = get_config_string(loader, "postgresql_client_auth.method", "basic");
			config.postgresqlClientAuthRequired = get_config_value<bool>(loader, "postgresql_client_auth.required", false);
			config.postgresqlClientAuthDatabase = get_config_string(loader, "postgresql_client_auth.database", "fauxdb");
			config.postgresqlClientAuthUsername = get_config_string(loader, "postgresql_client_auth.username", "");
			config.postgresqlClientAuthPassword = get_config_string(loader, "postgresql_client_auth.password", "");
			config.postgresqlClientAuthUseSSL = get_config_value<bool>(loader, "postgresql_client_auth.use_ssl", false);
			config.postgresqlClientAuthSSLCert = get_config_string(loader, "postgresql_client_auth.ssl_cert", "");
			config.postgresqlClientAuthSSLKey = get_config_string(loader, "postgresql_client_auth.ssl_key", "");
			config.postgresqlClientAuthSSLCA = get_config_string(loader, "postgresql_client_auth.ssl_ca", "");
		}
		else
		{
			config.setDefaults();
		}

		if (daemonMode || config.daemonMode)
		{
			std::cout << "Starting FauxDB in daemon mode..." << std::endl;

			if (getcwd(cwd, sizeof(cwd)) != NULL)
				pidFile = std::string(cwd) + "/fauxdb.pid";
			else
				pidFile = "/tmp/fauxdb.pid";

			if (!daemonize(pidFile))
			{
				std::cerr << "Failed to daemonize process" << std::endl;
				return 1;
			}
		}

		logFile = "fauxdb.log";
		loggerPtr = std::make_shared<CLogger>(config);
		loggerPtr->enableConsoleOutput(!(daemonMode || config.daemonMode));
		loggerPtr->setLogLevel(CLogLevel::DEBUG);
		loggerPtr->setLogFile(logFile);
		loggerPtr->initialize();

		server.setLogger(loggerPtr);

		if (!server.initialize(config))
		{
			lastError = server.getLastError();
			loggerPtr->log(CLogLevel::ERROR,
						   "FauxDB failed to initialize: " + lastError);
			return 1;
		}

		if (!server.start())
		{
			loggerPtr->log(CLogLevel::ERROR,
						   "FauxDB failed to start server process");
			return 1;
		}

		loggerPtr->log(CLogLevel::INFO,
					   "FauxDB started on " + config.bindAddress + ":" +
					   std::to_string(config.port));

		while (!signal.shouldExit())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			if (signal.shouldReload())
			{
				loggerPtr->log(CLogLevel::INFO, "Received reload signal");
				server.reloadConfiguration();
				signal.clearReloadFlag();
			}
		}

		loggerPtr->log(CLogLevel::INFO, "FauxDB shutdown complete");
		loggerPtr->shutdown();
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}
}