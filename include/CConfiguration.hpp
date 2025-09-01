#pragma once

#include "CServer.hpp"
#include "IInterfaces.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace FauxDB
{

class CConfiguration
{
  public:
    explicit CConfiguration(const std::string& configFile = "");
    virtual ~CConfiguration();
};

class CConfigurationParser
{
  public:
    CConfigurationParser();
    virtual ~CConfigurationParser() = default;
};

class CConfigurationValidator
{
  public:
    CConfigurationValidator();
    virtual ~CConfigurationValidator() = default;
};

class CConfigurationEncryption
{
  public:
    CConfigurationEncryption();
    virtual ~CConfigurationEncryption() = default;
};

class CConfigurationFactory
{
  public:
    enum class ConfigurationType
    {
        File,
        Database,
        Environment,
        Network,
        Custom
    };

    static std::unique_ptr<CConfiguration>
    createConfiguration(ConfigurationType type, const std::string& source);
    static std::string getConfigurationTypeName(ConfigurationType type);
    static ConfigurationType
    getConfigurationTypeFromString(const std::string& typeName);
};

} /* namespace FauxDB */
