/*-------------------------------------------------------------------------
 *
 * CHelp.hpp
 *      Command line help handler for FauxDB.
 *      Provides help information and usage instructions.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace FauxDB::CommandLine
{

using namespace FauxDB;

/**
 * Help topic categories
 */
enum class CHelpTopicCategory : uint8_t
{
	General = 0,
	Configuration = 1,
	Database = 2,
	Network = 3,
	Protocol = 4,
	Query = 5,
	Response = 6,
	Logging = 7,
	Security = 8,
	Development = 9
};

/**
 * Help topic structure
 */
struct CHelpTopic
{
	std::string name;
	std::string shortDescription;
	std::string longDescription;
	std::vector<std::string> examples;
	std::vector<std::string> options;
	std::vector<std::string> relatedTopics;
	CHelpTopicCategory category;

	CHelpTopic() : category(CHelpTopicCategory::General)
	{
	}
};

/**
 * Command line help handler
 * Provides help information and usage instructions
 */
class CHelp
{
  public:
	CHelp();
	virtual ~CHelp();
// ...existing code...

	/* Core help interface */
	virtual void showGeneralHelp() const;
	virtual void showTopicHelp(const std::string& topic) const;
	virtual void showCategoryHelp(CHelpTopicCategory category) const;
	virtual void showCommandHelp(const std::string& command) const;

	/* Help topic management */
	virtual void addTopic(const CHelpTopic& topic);
	virtual void removeTopic(const std::string& topicName);
	virtual void updateTopic(const std::string& topicName,
							 const CHelpTopic& topic);
	virtual CHelpTopic getTopic(const std::string& topicName) const;

	/* Help content generation */
	virtual std::string generateHelpText(const std::string& topic) const;
	virtual std::string generateUsageText(const std::string& command) const;
	virtual std::string generateExampleText(const std::string& topic) const;
	virtual std::string generateOptionText(const std::string& command) const;

	/* Help search and navigation */
	virtual std::vector<std::string>
	searchTopics(const std::string& query) const;
	virtual std::vector<std::string>
	getTopicsByCategory(CHelpTopicCategory category) const;
	virtual std::vector<std::string>
	getRelatedTopics(const std::string& topic) const;
	virtual std::vector<std::string> getAllTopics() const;

	/* Help formatting */
	virtual void setOutputFormat(const std::string& format);
	virtual void setIndentSize(int size);
	virtual void setLineWidth(int width);
	virtual void setColorOutput(bool enabled);

	/* Help customization */
	virtual void setApplicationName(const std::string& appName);
	virtual void setApplicationVersion(const std::string& version);
	virtual void setApplicationDescription(const std::string& description);
	virtual void setContactInfo(const std::string& contactInfo);

	/* Help export */
	virtual bool exportToFile(const std::string& filename,
							  const std::string& format) const;
	virtual bool exportToHTML(const std::string& filename) const;
	virtual bool exportToMarkdown(const std::string& filename) const;
	virtual bool exportToText(const std::string& filename) const;

	/* Help validation */
	virtual bool validateTopic(const CHelpTopic& topic) const;
	virtual bool validateTopicName(const std::string& topicName) const;
	virtual std::vector<std::string>
	getTopicValidationErrors(const CHelpTopic& topic) const;

	/* Help statistics */
	virtual size_t getTotalTopics() const;
	virtual size_t getTopicsByCategoryCount(CHelpTopicCategory category) const;
	virtual std::string getHelpStatistics() const;

  protected:
	/* Help state */
	std::unordered_map<std::string, CHelpTopic> topics_;
	std::unordered_map<CHelpTopicCategory, std::vector<std::string>>
		categoryTopics_;

	/* Help configuration */
	std::string outputFormat_;
	int indentSize_;
	int lineWidth_;
	bool colorOutput_;

	/* Application information */
	std::string applicationName_;
	std::string applicationVersion_;
	std::string applicationDescription_;
	std::string contactInfo_;

	/* Protected utility methods */
	virtual void initializeDefaultTopics();
	virtual void categorizeTopic(const CHelpTopic& topic);
	virtual void decategorizeTopic(const std::string& topicName);
	virtual std::string formatHelpText(const std::string& text) const;

	/* Help formatting helpers */
	virtual std::string formatTopicHeader(const CHelpTopic& topic) const;
	virtual std::string formatTopicDescription(const CHelpTopic& topic) const;
	virtual std::string formatTopicExamples(const CHelpTopic& topic) const;
	virtual std::string formatTopicOptions(const CHelpTopic& topic) const;
	virtual std::string formatTopicRelated(const CHelpTopic& topic) const;

	/* Help search helpers */
	virtual bool topicMatchesQuery(const CHelpTopic& topic,
								   const std::string& query) const;
	virtual std::vector<std::string>
	tokenizeQuery(const std::string& query) const;
	virtual bool
	topicContainsTokens(const CHelpTopic& topic,
						const std::vector<std::string>& tokens) const;

  private:
	/* Private state */
	std::vector<std::string> searchHistory_;
	std::unordered_map<std::string, size_t> topicUsageCount_;

	/* Private utility methods */
	void initializeDefaults();
	void cleanupState();

	/* Help export helpers */
	bool writeToFile(const std::string& filename,
					 const std::string& content) const;
	std::string generateHTMLContent() const;
	std::string generateMarkdownContent() const;
	std::string generateTextContent() const;

	/* Help validation helpers */
	bool validateTopicNameFormat(const std::string& topicName) const;
	bool validateTopicDescription(const std::string& description) const;
	bool validateTopicExamples(const std::vector<std::string>& examples) const;
	bool validateTopicOptions(const std::vector<std::string>& options) const;

	/* Utility methods */
	std::string buildErrorMessage(const std::string& operation,
								  const std::string& details) const;
	void logTopicUsage(const std::string& topicName) const;
	std::string getCategoryName(CHelpTopicCategory category) const;
};

} /* namespace FauxDB::CommandLine */
