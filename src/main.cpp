/*-------------------------------------------------------------------------
 *
 * main.cpp
 *      Main entry point for FauxDB server.
 *      Handles server initialization, configuration, and daemon mode.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
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

/* Helper function to get nested configuration values */
template <typename T>
T GetConfigValue(const FauxDB::CConfig& loader, const std::string& key,
                 const T& defaultValue)
{
    /* Try direct key first */
    if (auto value = loader.get(key))
    {
        if (std::holds_alternative<T>(*value))
        {
            return std::get<T>(*value);
        }
        else if (std::holds_alternative<std::string>(*value))
        {
            try
            {
                if constexpr (std::is_same_v<T, int>)
                {
                    return std::stoi(std::get<std::string>(*value));
                }
                else if constexpr (std::is_same_v<T, uint64_t>)
                {
                    return std::stoull(std::get<std::string>(*value));
                }
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

    /* Try nested keys for JSON/YAML format */
    std::vector<std::string> nestedKeys = {"server." + key, "postgresql." + key,
                                           "logging." + key, "security." + key,
                                           "performance." + key};

    for (const auto& nestedKey : nestedKeys)
    {
        if (auto value = loader.get(nestedKey))
        {
            if (std::holds_alternative<T>(*value))
            {
                return std::get<T>(*value);
            }
            else if (std::holds_alternative<std::string>(*value))
            {
                try
                {
                    if constexpr (std::is_same_v<T, int>)
                    {
                        return std::stoi(std::get<std::string>(*value));
                    }
                    else if constexpr (std::is_same_v<T, uint64_t>)
                    {
                        return std::stoull(std::get<std::string>(*value));
                    }
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

std::string GetConfigString(const FauxDB::CConfig& loader,
                            const std::string& key,
                            const std::string& defaultValue)
{
    /* Try direct key first */
    if (auto value = loader.get(key))
    {
        if (std::holds_alternative<std::string>(*value))
        {
            return std::get<std::string>(*value);
        }
    }

    /* Try nested keys for JSON/YAML format */
    std::vector<std::string> nestedKeys = {"server." + key, "postgresql." + key,
                                           "logging." + key, "security." + key,
                                           "performance." + key};

    for (const auto& nestedKey : nestedKeys)
    {
        if (auto value = loader.get(nestedKey))
        {
            if (std::holds_alternative<std::string>(*value))
            {
                return std::get<std::string>(*value);
            }
        }
    }

    return defaultValue;
}

using namespace FauxDB;
using namespace std::chrono;

/* Function to daemonize the process */
bool Daemonize(const std::string& pidFile = "/var/run/fauxdb.pid")
{
    /* First fork */
    pid_t pid = fork();
    if (pid < 0)
    {
        return false;
    }
    if (pid > 0)
    {
        /* Parent process exits */
        exit(0);
    }

    /* Create new session */
    if (setsid() < 0)
    {
        return false;
    }

    /* Second fork */
    pid = fork();
    if (pid < 0)
    {
        return false;
    }
    if (pid > 0)
    {
        /* Parent process exits */
        exit(0);
    }

    /* Set file permissions */
    umask(0);

    /* Create PID file before changing directory */
    FILE* pidFileHandle = fopen(pidFile.c_str(), "w");
    if (pidFileHandle)
    {
        fprintf(pidFileHandle, "%d\n", getpid());
        fclose(pidFileHandle);
    }

    /* Change to root directory */
    chdir("/");

    /* Close standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* Redirect standard file descriptors to /dev/null */
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);

    return true;
}

int main(int argc, char* argv[])
{
    std::string configFile;
    bool daemonMode = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            std::cout << "FauxDB - Document Database Engine to PostgreSQL\n";
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n\n";
            std::cout << "Options:\n";
            std::cout << "  -c, --config <file>    Configuration file "
                         "(supports .conf, .json, .yaml, .yml)\n";
            std::cout
                << "  -d, --daemon           Run in daemon mode (background)\n";
            std::cout << "  -h, --help             Show this help message\n";
            std::cout
                << "  -v, --version          Show version information\n\n";
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
            std::cout << "MongoDB to PostgreSQL Query Translator\n";
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
            /* Use cerr for command line errors since logger isn't available yet
             */
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
                /* Use cerr for config loading errors since logger isn't
                 * available yet */
                std::cerr << "Failed to load config file: " << configFile
                          << ", error=" << err.message() << std::endl;
                return 1;
            }

            /* Load configuration using helper functions */
            config.port = GetConfigValue<int>(loader, "port", 27017);
            config.bindAddress =
                GetConfigString(loader, "bind_address", "0.0.0.0");
            config.maxConnections =
                GetConfigValue<int>(loader, "max_connections", 1000);
            config.workerThreads =
                GetConfigValue<int>(loader, "worker_threads", 4);
            config.logLevel = GetConfigString(loader, "log_level", "INFO");

            /* PostgreSQL configuration */
            config.pgHost =
                GetConfigString(loader, "postgresql.host", "localhost");
            config.pgPort = GetConfigString(loader, "postgresql.port", "5432");
            config.pgDatabase =
                GetConfigString(loader, "postgresql.database", "fauxdb");
            config.pgUser =
                GetConfigString(loader, "postgresql.user", "fauxdb");
            config.pgPassword =
                GetConfigString(loader, "postgresql.password", "fauxdb");
            config.pgPoolSize =
                GetConfigValue<uint64_t>(loader, "postgresql.pool_size", 10);

            /* Convert timeout to milliseconds */
            int timeoutSeconds =
                GetConfigValue<int>(loader, "postgresql.timeout", 10);
            config.pgTimeout = std::chrono::milliseconds(timeoutSeconds * 1000);

            /* Load daemon mode setting */
            config.daemonMode =
                GetConfigValue<bool>(loader, "daemon_mode", false);

            /* MongoDB server-side authentication (FauxDB as MongoDB server) */
            config.mongodbServerAuthMethod = GetConfigString(
                loader, "mongodb_server_auth.method",
                GetConfigString(loader, "mongodb_server_auth_method",
                                "scram-sha-256"));
            config.mongodbServerAuthRequired = GetConfigValue<bool>(
                loader, "mongodb_server_auth.required",
                GetConfigValue<bool>(loader, "mongodb_server_auth_required",
                                     false));
            config.mongodbServerAuthDatabase = GetConfigString(
                loader, "mongodb_server_auth.database",
                GetConfigString(loader, "mongodb_server_auth_database",
                                "admin"));
            config.mongodbServerAuthUsername = GetConfigString(
                loader, "mongodb_server_auth.username",
                GetConfigString(loader, "mongodb_server_auth_username", ""));
            config.mongodbServerAuthPassword = GetConfigString(
                loader, "mongodb_server_auth.password",
                GetConfigString(loader, "mongodb_server_auth_password", ""));
            config.mongodbServerAuthUseSSL = GetConfigValue<bool>(
                loader, "mongodb_server_auth.use_ssl",
                GetConfigValue<bool>(loader, "mongodb_server_auth_use_ssl",
                                     false));
            config.mongodbServerAuthSSLCert = GetConfigString(
                loader, "mongodb_server_auth.ssl_cert",
                GetConfigString(loader, "mongodb_server_auth_ssl_cert", ""));
            config.mongodbServerAuthSSLKey = GetConfigString(
                loader, "mongodb_server_auth.ssl_key",
                GetConfigString(loader, "mongodb_server_auth_ssl_key", ""));
            config.mongodbServerAuthSSLCA = GetConfigString(
                loader, "mongodb_server_auth.ssl_ca",
                GetConfigString(loader, "mongodb_server_auth_ssl_ca", ""));

            /* PostgreSQL client-side authentication (FauxDB to PostgreSQL) */
            config.postgresqlClientAuthMethod = GetConfigString(
                loader, "postgresql_client_auth.method",
                GetConfigString(loader, "postgresql_client_auth_method",
                                "basic"));
            config.postgresqlClientAuthRequired = GetConfigValue<bool>(
                loader, "postgresql_client_auth.required",
                GetConfigValue<bool>(loader, "postgresql_client_auth_required",
                                     false));
            config.postgresqlClientAuthDatabase = GetConfigString(
                loader, "postgresql_client_auth.database",
                GetConfigString(loader, "postgresql_client_auth_database",
                                "fauxdb"));
            config.postgresqlClientAuthUsername = GetConfigString(
                loader, "postgresql_client_auth.username",
                GetConfigString(loader, "postgresql_client_auth_username", ""));
            config.postgresqlClientAuthPassword = GetConfigString(
                loader, "postgresql_client_auth.password",
                GetConfigString(loader, "postgresql_client_auth_password", ""));
            config.postgresqlClientAuthUseSSL = GetConfigValue<bool>(
                loader, "postgresql_client_auth.use_ssl",
                GetConfigValue<bool>(loader, "postgresql_client_auth_use_ssl",
                                     false));
            config.postgresqlClientAuthSSLCert = GetConfigString(
                loader, "postgresql_client_auth.ssl_cert",
                GetConfigString(loader, "postgresql_client_auth_ssl_cert", ""));
            config.postgresqlClientAuthSSLKey = GetConfigString(
                loader, "postgresql_client_auth.ssl_key",
                GetConfigString(loader, "postgresql_client_auth_ssl_key", ""));
            config.postgresqlClientAuthSSLCA = GetConfigString(
                loader, "postgresql_client_auth.ssl_ca",
                GetConfigString(loader, "postgresql_client_auth_ssl_ca", ""));

            /* Debug: Print loaded PostgreSQL configuration */
            std::cout << "DEBUG: Loaded PostgreSQL config - host: "
                      << config.pgHost << ", port: " << config.pgPort
                      << ", database: " << config.pgDatabase
                      << ", user: " << config.pgUser << std::endl;

            /* Debug: Print loaded authentication configuration */
            std::cout << "DEBUG: Loaded MongoDB server auth config - method: "
                      << config.mongodbServerAuthMethod << ", required: "
                      << (config.mongodbServerAuthRequired ? "true" : "false")
                      << ", database: " << config.mongodbServerAuthDatabase
                      << std::endl;
            std::cout
                << "DEBUG: Loaded PostgreSQL client auth config - method: "
                << config.postgresqlClientAuthMethod << ", required: "
                << (config.postgresqlClientAuthRequired ? "true" : "false")
                << ", database: " << config.postgresqlClientAuthDatabase
                << std::endl;
        }
        else
        {
            config.setDefaults();
        }

        /* Daemonize if requested (command-line takes precedence over config) */
        if (daemonMode || config.daemonMode)
        {
            std::cout << "Starting FauxDB in daemon mode..." << std::endl;
            /* Get current working directory for PID file */
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL)
            {
                std::string pidFile = std::string(cwd) + "/fauxdb.pid";
                if (!Daemonize(pidFile))
                {
                    std::cerr << "Failed to daemonize process" << std::endl;
                    return 1;
                }
            }
            else
            {
                if (!Daemonize("/tmp/fauxdb.pid"))
                {
                    std::cerr << "Failed to daemonize process" << std::endl;
                    return 1;
                }
            }
            std::cout << "FauxDB daemonized successfully. PID: " << getpid()
                      << std::endl;
        }

        std::string logFile = "fauxdb.log";
        auto loggerPtr = std::make_shared<CLogger>(config);
        loggerPtr->enableConsoleOutput(!(daemonMode || config.daemonMode));
        loggerPtr->setLogLevel(CLogLevel::DEBUG);
        loggerPtr->setLogFile(logFile);
        loggerPtr->initialize();

        CServer server;
        server.setLogger(loggerPtr);
        // Only log actionable or unique events.
        try
        {
            if (!server.initialize(config))
            {
                std::string lastError = server.getLastError();
                loggerPtr->log(CLogLevel::ERROR,
                               "FauxDB daemon failed to initialize server with "
                               "config: address=" +
                                   config.bindAddress + ", port=" +
                                   std::to_string(config.port) + ", threads=" +
                                   std::to_string(config.workerThreads) +
                                   ", error: " + lastError);
                return 1;
            }
        }
        catch (const std::exception& e)
        {
            loggerPtr->log(
                CLogLevel::ERROR,
                "FauxDB daemon failed to initialize server with exception: " +
                    std::string(e.what()));
            return 1;
        }

        if (!server.start())
        {
            loggerPtr->log(
                CLogLevel::ERROR,
                "FauxDB daemon failed to start server process. Check "
                "network/database configuration and logs for details.");
            return 1;
        }

        loggerPtr->log(CLogLevel::INFO,
                       "FauxDB daemon started. Listening on " +
                           config.bindAddress + ":" +
                           std::to_string(config.port) + ", worker threads=" +
                           std::to_string(config.workerThreads) + ".");
        loggerPtr->log(CLogLevel::INFO,
                       "Server info: " + server.getServerInfo() + ".");
        loggerPtr->log(CLogLevel::INFO,
                       "Database status: " + server.getDatabaseStatus() + ".");
        loggerPtr->log(CLogLevel::INFO,
                       "Network status: " + server.getNetworkStatus() + ".");
        loggerPtr->log(CLogLevel::INFO, "Initial server statistics: " +
                                            server.getServerStatistics() + ".");

        static std::atomic<bool> shutdownRequested{false};
        auto signalHandlerFunc = [](int signal) { shutdownRequested = true; };
        std::signal(SIGINT, signalHandlerFunc);
        std::signal(SIGTERM, signalHandlerFunc);

        bool shutdownLogged = false;
        while (true)
        {
            if (shutdownRequested && !shutdownLogged)
            {
                loggerPtr->log(
                    CLogLevel::INFO,
                    "Shutdown signal received. Initiating shutdown sequence.");
                shutdownLogged = true;
            }
            if (!server.isRunning() || shutdownRequested)
            {
                break;
            }
            static auto lastStatusUpdate = steady_clock::now();
            auto now = steady_clock::now();
            if (duration_cast<seconds>(now - lastStatusUpdate).count() >= 30)
            {
                loggerPtr->log(
                    CLogLevel::INFO,
                    "[Status] FauxDB daemon periodic update: database "
                    "status: " +
                        server.getDatabaseStatus() +
                        ", network status: " + server.getNetworkStatus() + ".");
                lastStatusUpdate = now;
            }
            std::this_thread::sleep_for(milliseconds(100));
        }
        loggerPtr->log(CLogLevel::INFO, "Shutdown requested. Stopping FauxDB "
                                        "server and cleaning up resources.");
        server.shutdown();
        loggerPtr->log(CLogLevel::INFO,
                       "FauxDB daemon shutdown complete. All resources "
                       "released and server stopped.");
        loggerPtr->shutdown();
        return 0;
    }
    catch (const std::exception& e)
    {
        /* Use cerr for fatal errors since logger might not be available */
        std::cerr << "[Fatal] Fatal error: " << e.what() << std::endl;
        return 1;
    }
}