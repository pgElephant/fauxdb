/*-------------------------------------------------------------------------
 *
 * CConfig.cpp
 *      Implementation for FauxDB.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */


// System headers
#include <fstream>
#include <iostream>
#include <sstream>

// Library headers
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

// Project headers
#include "CConfig.hpp"
#include "CLogger.hpp"

namespace FauxDB
{

CConfig::CConfig()
    : logger_(nullptr),
      config_values_(
          std::make_unique<std::unordered_map<std::string, ConfigValue>>()),
      config_metadata_(
          std::make_unique<std::unordered_map<std::string, ConfigEntry>>()),
      initialized_(false), hot_reload_enabled_(false)
{
    config_values_ =
        std::make_unique<std::unordered_map<std::string, ConfigValue>>();
    config_metadata_ =
        std::make_unique<std::unordered_map<std::string, ConfigEntry>>();
    initialized_ = true;
}

CConfig::~CConfig() = default;

std::error_code CConfig::loadFromFile(const std::string& filename)
{
    if (!initialized_)
    {
        return std::make_error_code(std::errc::invalid_argument);
    }

    std::string extension = filename.substr(filename.find_last_of('.') + 1);
    if (extension == "json")
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return std::make_error_code(std::errc::no_such_file_or_directory);
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();
        return loadFromJson(content);
    }
    else if (extension == "yaml" || extension == "yml")
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return std::make_error_code(std::errc::no_such_file_or_directory);
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();
        return loadFromYaml(content);
    }
    else if (extension == "toml")
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return std::make_error_code(std::errc::no_such_file_or_directory);
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();
        return loadFromToml(content);
    }
    else if (extension == "ini" || extension == "conf")
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return std::make_error_code(std::errc::no_such_file_or_directory);
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();
        return loadFromIni(content);
    }

    return std::make_error_code(std::errc::invalid_argument);
}

std::error_code CConfig::loadFromJson(const std::string& jsonContent)
{
    if (jsonContent.empty())
    {
        return std::make_error_code(std::errc::invalid_argument);
    }

    try
    {
        nlohmann::json j = nlohmann::json::parse(jsonContent);

        // Recursively process JSON nodes to flatten nested structure
        processJsonNode("", j);

        return std::error_code{};
    }
    catch (const nlohmann::json::exception& e)
    {
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         std::string("JSON parsing error: '") + e.what() +
                             "'.");
        }
        return std::make_error_code(std::errc::invalid_argument);
    }
    catch (const std::exception& e)
    {
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         std::string("JSON loading error: '") + e.what() +
                             "'.");
        }
        return std::make_error_code(std::errc::invalid_argument);
    }
}

void CConfig::processJsonNode(const std::string& prefix,
                              const nlohmann::json& node)
{
    if (node.is_object())
    {
        for (auto it = node.begin(); it != node.end(); ++it)
        {
            std::string key = it.key();
            std::string fullKey = prefix.empty() ? key : prefix + "." + key;
            processJsonNode(fullKey, it.value());
        }
    }
    else if (node.is_string())
    {
        set(prefix, node.get<std::string>());
    }
    else if (node.is_number_integer())
    {
        set(prefix, node.get<int>());
    }
    else if (node.is_number_float())
    {
        set(prefix, node.get<double>());
    }
    else if (node.is_boolean())
    {
        set(prefix, node.get<bool>());
    }
    else if (node.is_array())
    {
        std::vector<std::string> arrayValues;
        for (const auto& item : node)
        {
            if (item.is_string())
            {
                arrayValues.push_back(item.get<std::string>());
            }
            else
            {
                arrayValues.push_back(item.dump());
            }
        }
        set(prefix, arrayValues);
    }
    else
    {
        // Convert other types to string
        set(prefix, node.dump());
    }
}

std::error_code CConfig::loadFromYaml(const std::string& yamlContent)
{
    if (yamlContent.empty())
    {
        return std::make_error_code(std::errc::invalid_argument);
    }

    try
    {
        YAML::Node config = YAML::Load(yamlContent);

        // Recursively process YAML nodes
        processYamlNode("", config);

        return std::error_code{};
    }
    catch (const YAML::Exception& e)
    {
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         std::string("YAML parsing error: '") + e.what() +
                             "'.");
        }
        return std::make_error_code(std::errc::invalid_argument);
    }
    catch (const std::exception& e)
    {
        if (logger_)
        {
            logger_->log(CLogLevel::ERROR,
                         std::string("YAML loading error: '") + e.what() +
                             "'.");
        }
        return std::make_error_code(std::errc::invalid_argument);
    }
}

void CConfig::processYamlNode(const std::string& prefix, const YAML::Node& node)
{
    if (node.IsMap())
    {
        for (const auto& pair : node)
        {
            std::string key = pair.first.as<std::string>();
            std::string fullKey = prefix.empty() ? key : prefix + "." + key;
            processYamlNode(fullKey, pair.second);
        }
    }
    else if (node.IsScalar())
    {
        if (node.IsNull())
        {
            set(prefix, std::string(""));
        }
        else
        {
            // Try to determine the type
            std::string strValue = node.as<std::string>();

            // Check if it's a boolean
            if (strValue == "true" || strValue == "false")
            {
                set(prefix, strValue == "true");
            }
            // Check if it's a number
            else
            {
                try
                {
                    if (strValue.find('.') != std::string::npos)
                    {
                        set(prefix, std::stod(strValue));
                    }
                    else
                    {
                        set(prefix, std::stoi(strValue));
                    }
                }
                catch (...)
                {
                    set(prefix, strValue);
                }
            }
        }
    }
    else if (node.IsSequence())
    {
        std::vector<std::string> arrayValues;
        for (const auto& item : node)
        {
            if (item.IsScalar())
            {
                arrayValues.push_back(item.as<std::string>());
            }
            else
            {
                // Convert complex types to string representation
                std::stringstream ss;
                ss << item;
                arrayValues.push_back(ss.str());
            }
        }
        set(prefix, arrayValues);
    }
}

std::error_code CConfig::loadFromToml(const std::string& tomlContent)
{
    if (tomlContent.empty())
    {
        return std::make_error_code(std::errc::invalid_argument);
    }

    try
    {
        std::istringstream stream(tomlContent);
        std::string line;

        while (std::getline(stream, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            size_t equalPos = line.find('=');
            if (equalPos == std::string::npos)
                continue;

            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (!key.empty() && !value.empty())
            {
                if (value[0] == '"' && value[value.length() - 1] == '"')
                {
                    value = value.substr(1, value.length() - 2);
                    set(key, value);
                }
                else if (value == "true" || value == "false")
                {
                    set(key, value == "true");
                }
                else
                {
                    try
                    {
                        double numValue = std::stod(value);
                        set(key, numValue);
                    }
                    catch (...)
                    {
                        set(key, value);
                    }
                }
            }
        }

        return std::error_code{};
    }
    catch (const std::exception& e)
    {
        return std::make_error_code(std::errc::invalid_argument);
    }
}

std::error_code CConfig::loadFromIni(const std::string& iniContent)
{
    if (iniContent.empty())
    {
        return std::make_error_code(std::errc::invalid_argument);
    }

    try
    {
        std::istringstream stream(iniContent);
        std::string line;
        std::string currentSection;

        while (std::getline(stream, line))
        {
            if (line.empty() || line[0] == ';' || line[0] == '#')
                continue;

            if (line[0] == '[')
            {
                size_t endBracket = line.find(']');
                if (endBracket != std::string::npos)
                {
                    currentSection = line.substr(1, endBracket - 1);
                }
                continue;
            }

            size_t equalPos = line.find('=');
            if (equalPos == std::string::npos)
                continue;

            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (!key.empty() && !value.empty())
            {
                std::string fullKey =
                    currentSection.empty() ? key : currentSection + "." + key;
                set(fullKey, value);
            }
        }

        return std::error_code{};
    }
    catch (const std::exception& e)
    {
        return std::make_error_code(std::errc::invalid_argument);
    }
}

std::string CConfig::toJson() const
{
    try
    {
        nlohmann::json j;

        for (const auto& [key, value] : *config_values_)
        {
            std::visit(
                [&j, &key](const auto& v)
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        j[key] = v;
                    }
                    else if constexpr (std::is_same_v<T,
                                                      std::vector<std::string>>)
                    {
                        j[key] = v;
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        j[key] = v;
                    }
                    else if constexpr (std::is_same_v<T, int> ||
                                       std::is_same_v<T, int64_t> ||
                                       std::is_same_v<T, uint64_t>)
                    {
                        j[key] = v;
                    }
                    else if constexpr (std::is_same_v<T, double>)
                    {
                        j[key] = v;
                    }
                    else
                    {
                        j[key] = std::to_string(v);
                    }
                },
                value);
        }

        return j.dump(2); // Pretty print with 2 spaces indentation
    }
    catch (const std::exception& e)
    {
        if (logger_)
        {
            logger_->log(
                CLogLevel::ERROR,
                std::string("Failed to convert configuration to JSON: ") +
                    e.what());
        }
        return "{}";
    }
}

std::string CConfig::toYaml() const
{
    try
    {
        std::string yaml;
        for (const auto& [key, value] : *config_values_)
        {
            yaml += key + ":\n";

            std::visit(
                [&yaml](const auto& v)
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        yaml += "  " + v + "\n";
                    }
                    else if constexpr (std::is_same_v<T,
                                                      std::vector<std::string>>)
                    {
                        for (const auto& item : v)
                        {
                            yaml += "  - " + item + "\n";
                        }
                    }
                    else
                    {
                        yaml += "  " + std::to_string(v) + "\n";
                    }
                },
                value);
        }
        return yaml;
    }
    catch (const std::exception& e)
    {
        if (logger_)
        {
            logger_->log(
                CLogLevel::ERROR,
                std::string("Failed to convert configuration to YAML: ") +
                    e.what());
        }
        return "";
    }
}

std::string CConfig::toToml() const
{
    try
    {
        std::string toml;
        for (const auto& [key, value] : *config_values_)
        {
            toml += key + " = ";

            std::visit(
                [&toml](const auto& v)
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        toml += "\"" + v + "\"";
                    }
                    else if constexpr (std::is_same_v<T,
                                                      std::vector<std::string>>)
                    {
                        toml += "[";
                        for (size_t i = 0; i < v.size(); ++i)
                        {
                            if (i > 0)
                                toml += ", ";
                            toml += "\"" + v[i] + "\"";
                        }
                        toml += "]";
                    }
                    else
                    {
                        toml += std::to_string(v);
                    }
                },
                value);

            toml += "\n";
        }
        return toml;
    }
    catch (const std::exception& e)
    {
        if (logger_)
        {
            logger_->log(
                CLogLevel::ERROR,
                std::string("Failed to convert configuration to TOML: ") +
                    e.what());
        }
        return "";
    }
}

void CConfig::set(const std::string& key, const ConfigValue& value)
{
    (*config_values_)[key] = value;

    ConfigEntry entry;
    entry.key = key;
    entry.value = value;
    entry.lastModified = std::chrono::system_clock::now();
    entry.source = ConfigSource::RUNTIME;

    (*config_metadata_)[key] = entry;

    if (logger_)
    {
        logger_->log(CLogLevel::INFO,
                     "Configuration value set: '" + key + "'.");
    }
}

std::optional<ConfigValue> CConfig::get(const std::string& key) const
{
    auto it = config_values_->find(key);
    if (it != config_values_->end())
    {
        return it->second;
    }
    return std::nullopt;
}

bool CConfig::has(const std::string& key) const
{
    return config_values_->find(key) != config_values_->end();
}

std::vector<std::string> CConfig::keys() const
{
    std::vector<std::string> result;
    result.reserve(config_values_->size());
    for (const auto& [key, _] : *config_values_)
    {
        result.push_back(key);
    }
    return result;
}

std::error_code CConfig::enableHotReload(bool enable)
{
    hot_reload_enabled_ = enable;
    if (logger_)
    {
        logger_->log(CLogLevel::INFO, std::string("Hot reload ") +
                                          (enable ? "enabled" : "disabled") +
                                          " for configuration");
    }
    return std::error_code{};
}

std::error_code CConfig::watchFile(const std::string& filename)
{
    if (!hot_reload_enabled_)
    {
        return std::make_error_code(std::errc::operation_not_supported);
    }

    watched_files_.insert(filename);
    if (logger_)
    {
        logger_->log(CLogLevel::INFO,
                     "Watching configuration file: " + filename);
    }
    return std::error_code{};
}

} // namespace FauxDB
