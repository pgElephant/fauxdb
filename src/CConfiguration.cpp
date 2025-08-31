/*-------------------------------------------------------------------------
 *
 * CConfiguration.cpp
 *      Configuration management implementation for FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "CConfiguration.hpp"

namespace FauxDB
{

CConfiguration::CConfiguration(const std::string& configFile)
{
}

CConfiguration::~CConfiguration()
{
}

CConfigurationParser::CConfigurationParser()
{
}

CConfigurationValidator::CConfigurationValidator()
{
}

CConfigurationEncryption::CConfigurationEncryption()
{
}

std::unique_ptr<CConfiguration> CConfigurationFactory::createConfiguration(ConfigurationType type, const std::string& source)
{
    return std::make_unique<CConfiguration>(source);
}

std::string CConfigurationFactory::getConfigurationTypeName(ConfigurationType type)
{
    switch (type)
    {
        case ConfigurationType::File:
            return "File";
        case ConfigurationType::Database:
            return "Database"; 
        case ConfigurationType::Environment:
            return "Environment";
        case ConfigurationType::Network:
            return "Network";
        case ConfigurationType::Custom:
            return "Custom";
        default:
            return "Unknown";
    }
}

CConfigurationFactory::ConfigurationType CConfigurationFactory::getConfigurationTypeFromString(const std::string& typeName)
{
    if (typeName == "File")
        return ConfigurationType::File;
    if (typeName == "Database")
        return ConfigurationType::Database;
    if (typeName == "Environment") 
        return ConfigurationType::Environment;
    if (typeName == "Network")
        return ConfigurationType::Network;
    if (typeName == "Custom")
        return ConfigurationType::Custom;
        
    throw std::invalid_argument("Invalid configuration type name");
}

} // namespace FauxDB
