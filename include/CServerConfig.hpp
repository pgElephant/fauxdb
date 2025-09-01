
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace FauxDB
{

enum class CServerStatus : uint8_t
{
    Stopped = 0,
    Starting = 1,
    Running = 2,
    Stopping = 3,
    Error = 4,
    Maintenance = 5
};

struct CServerConfig
{
    std::string serverName;
    std::string bindAddress;
    uint16_t port;
    size_t maxConnections;
    size_t workerThreads;
    std::chrono::milliseconds startupTimeout;
    std::chrono::milliseconds shutdownTimeout;
    bool enableSSL;
    std::string sslCertPath;
    std::string sslKeyPath;
    std::string logLevel;
    std::string configFile;

    std::string cloudProvider;
    std::string observabilityBackend;
    std::string compressionAlgorithm;
    std::string serializationFormat;
    bool enableMetrics;
    bool enableTracing;
    bool enableProfiling;
    bool enableHotReload;
    std::string configFormat;
    bool daemonMode;

    std::string pgHost;
    std::string pgPort;
    std::string pgDatabase;
    std::string pgUser;
    std::string pgPassword;
    size_t pgPoolSize;
    std::chrono::milliseconds pgTimeout;

    CServerConfig()
        : serverName("FauxDB"), bindAddress("0.0.0.0"), port(27017),
          maxConnections(100), workerThreads(4), startupTimeout(30000),
          shutdownTimeout(30000), enableSSL(false), sslCertPath(""),
          sslKeyPath(""), logLevel("INFO"), configFile(""),
          cloudProvider("none"), observabilityBackend("none"),
          compressionAlgorithm("none"), serializationFormat("json"),
          enableMetrics(false), enableTracing(false), enableProfiling(false),
          enableHotReload(false), configFormat("json"), daemonMode(false),
          pgHost("localhost"), pgPort("5432"), pgDatabase("fauxdb"),
          pgUser("fauxdb"), pgPassword("fauxdb"), pgPoolSize(10),
          pgTimeout(10000)
    {
    }

    void setDefaults();
    bool loadFromFile(const std::string& filename);
    bool validate() const;
};

} /* namespace FauxDB */
