
#pragma once

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <yaml-cpp/yaml.h>
#include "CLogger.hpp"
#include <vector>

namespace FauxDB
{

using ConfigValue = std::variant<std::string, int, int64_t, uint64_t, double,
								 bool, std::vector<std::string>>;

enum class ConfigSource
{
	FILE,
	RUNTIME,
	DEFAULT,
	ENVIRONMENT
};

struct ConfigEntry
{
	std::string key;
	ConfigValue value;
	std::string description;
	std::string category;
	std::chrono::system_clock::time_point lastModified;
	bool required;
	bool sensitive;
	ConfigSource source;

	ConfigEntry() : required(false), sensitive(false), source(ConfigSource::DEFAULT) {}
};

class CConfig
{
public:
	CConfig();
	~CConfig();

	std::error_code loadFromFile(const std::string& filename);
	std::error_code loadFromJson(const std::string& jsonContent);
	std::error_code loadFromYaml(const std::string& yamlContent);
	std::error_code loadFromToml(const std::string& tomlContent);
	std::error_code loadFromIni(const std::string& iniContent);

	void set(const std::string& key, const ConfigValue& value);
	std::optional<ConfigValue> get(const std::string& key) const;
	bool has(const std::string& key) const;
	std::vector<std::string> keys() const;

	std::error_code enableHotReload(bool enable);
	std::error_code watchFile(const std::string& filename);

	std::string toJson() const;
	std::string toYaml() const;
	std::string toToml() const;
	std::string toIni() const;

	void setLogger(std::shared_ptr<CLogger> logger);
	std::shared_ptr<CLogger> getLogger() const;

private:
	std::shared_ptr<CLogger> logger_;
	std::unique_ptr<std::unordered_map<std::string, ConfigValue>> config_values_;
	std::unique_ptr<std::unordered_map<std::string, ConfigEntry>> config_metadata_;
	bool initialized_;
	bool hot_reload_enabled_;
	std::unordered_set<std::string> watched_files_;

	void parseJsonValue(const std::string& key, const std::string& value);
	void parseYamlValue(const std::string& key, const std::string& value);
	void parseTomlValue(const std::string& key, const std::string& value);
	void parseIniValue(const std::string& key, const std::string& value);
	void processJsonNode(const std::string& prefix, const nlohmann::json& node);
	void processYamlNode(const std::string& prefix, const YAML::Node& node);
	std::string escapeString(const std::string& str) const;
	std::string unescapeString(const std::string& str) const;
};

} // namespace FauxDB

