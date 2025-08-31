/*-------------------------------------------------------------------------
 *
 * CQueryTranslator.hpp
 *      Query translation module for FauxDB.
 *      Handles translation of Document queries to SQL for different database
 * backends.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "../../CTypes.hpp"
#include "../../IInterfaces.hpp"
#include "../../database/CDatabase.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace FauxDB
{

/**
 * Concrete query translator implementation
 * Translates Document queries to SQL for PostgreSQL
 */
class CQueryTranslator
{
  public:
	explicit CQueryTranslator(
		std::shared_ptr<FauxDB::CDatabase> database);
	virtual ~CQueryTranslator();

	/* IQueryTranslator interface implementation */
	std::string translateDocumentQueryToSQL(const CQueryContext& context);
	std::string translateFilter(const std::string& documentFilter);
	std::string translateProjection(const std::string& documentProjection);
	std::string translateSort(const std::string& documentSort);
	std::string translateUpdate(const std::string& documentUpdate);
	std::string translateAggregation(const std::string& documentPipeline);

	/* Additional translation methods */
	std::string translateIndex(const std::string& documentIndex);
	std::string translateSchema(const std::string& documentSchema);
	std::string translateGeoQuery(const std::string& documentGeoQuery);
	std::string translateTextSearch(const std::string& documentTextQuery);

	/* Configuration */
	void setTargetDatabase(const std::string& databaseType);
	void setEscapeMode(bool escapeIdentifiers);
	void setQuoteStyle(const std::string& quoteStyle);

  private:
	/* Dependencies */
	std::shared_ptr<FauxDB::CDatabase> database_;
	std::string currentCollection_;
	std::string targetDatabaseType_;
	bool escapeIdentifiers_;
	std::string quoteStyle_;

	/* Translation state */
	std::unordered_map<std::string, std::string> fieldMappings_;
	std::vector<std::string> translatedFields_;

	/* Helper methods for specific Document operators */
	std::string translateComparisonOperator(const std::string& field,
											const std::string& value);
	std::string translateLogicalOperator(const std::string& operatorName,
										 const std::string& value);
	std::string translateArrayOperator(const std::string& field,
									   const std::string& value);
	std::string translateRegexOperator(const std::string& field,
									   const std::string& value);
	std::string translateGeoOperator(const std::string& field,
									 const std::string& value);
	std::string translateDateOperator(const std::string& field,
									  const std::string& value);
	std::string translateMathOperator(const std::string& field,
									  const std::string& value);

	/* Utility methods */
	std::string escapeSQLIdentifier(const std::string& identifier);
	std::string escapeSQLString(const std::string& str);
	std::string convertDocumentTypeToPostgreSQL(const std::string& documentType);
	std::string buildTableName(const std::string& database,
							   const std::string& collection);
	std::string buildColumnName(const std::string& field);

	/* JSON parsing helpers */
	bool isValidJSON(const std::string& jsonStr);
	std::string extractFieldValue(const std::string& jsonStr,
								  const std::string& fieldName);

	/* Utility helper function */
	std::string join(const std::vector<std::string>& elements,
					 const std::string& delimiter);

	/* Operator mapping tables */
	static const std::unordered_map<std::string, std::string>
		comparisonOperators_;
	static const std::unordered_map<std::string, std::string> logicalOperators_;
	static const std::unordered_map<std::string, std::string> arrayOperators_;
	static const std::unordered_map<std::string, std::string> geoOperators_;
};

/**
 * Query translator factory for creating different database-specific translators
 */
class CQueryTranslatorFactory
{
  public:
	enum class DatabaseType
	{
		PostgreSQL,
		MySQL,
		SQLite,
		SQLServer,
		Oracle
	};

	static std::unique_ptr<CQueryTranslator>
	createTranslator(DatabaseType type,
					 std::shared_ptr<FauxDB::CDatabase> database);

	static std::string getDatabaseTypeName(DatabaseType type);
	static DatabaseType getDatabaseTypeFromString(const std::string& typeName);
};

/**
 * Query optimization engine
 * Analyzes and optimizes translated SQL queries
 */
class CQueryOptimizer
{
  public:
	CQueryOptimizer();
	~CQueryOptimizer() = default;

	/* Query optimization methods */
	std::string optimizeQuery(const std::string& sqlQuery);
	std::string addIndexHints(const std::string& sqlQuery,
							  const std::vector<std::string>& indexes);
	std::string rewriteSubqueries(const std::string& sqlQuery);
	std::string optimizeJoins(const std::string& sqlQuery);

	/* Query analysis */
	struct QueryAnalysis
	{
		bool hasSubqueries;
		bool hasJoins;
		bool hasAggregations;
		bool hasOrderBy;
		bool hasGroupBy;
		int estimatedCost;
		std::vector<std::string> suggestedIndexes;
	};

	QueryAnalysis analyzeQuery(const std::string& sqlQuery);

	/* Configuration */
	void setOptimizationLevel(int level);
	void enableIndexHints(bool enable);
	void enableSubqueryRewriting(bool enable);

  private:
	/* Configuration */
	int optimizationLevel_;
	bool enableIndexHints_;
	bool enableSubqueryRewriting_;

	/* Private methods */
	std::string detectQueryType(const std::string& sqlQuery);
	std::vector<std::string> extractTableNames(const std::string& sqlQuery);
	std::vector<std::string> extractColumnNames(const std::string& sqlQuery);
	int estimateQueryCost(const std::string& sqlQuery);
};

/**
 * Query validation engine
 * Validates translated SQL queries for correctness and safety
 */
class CQueryValidator
{
  public:
	CQueryValidator();
	~CQueryValidator() = default;

	/* Validation methods */
	bool isValidSQL(const std::string& sqlQuery);
	bool isSafeQuery(const std::string& sqlQuery);
	std::string getValidationError() const;
	std::vector<std::string> getValidationWarnings() const;

	/* Security checks */
	bool hasSQLInjectionRisk(const std::string& sqlQuery);
	bool hasDangerousOperations(const std::string& sqlQuery);
	bool hasExcessiveComplexity(const std::string& sqlQuery);

	/* Configuration */
	void setMaxQueryLength(size_t maxLength);
	void setMaxComplexity(int maxComplexity);
	void enableStrictMode(bool enable);

  private:
	/* Configuration */
	size_t maxQueryLength_;
	int maxComplexity_;
	bool strictMode_;

	/* Validation state */
	std::string lastValidationError_;
	std::vector<std::string> validationWarnings_;

	/* Private methods */
	bool checkSyntax(const std::string& sqlQuery);
	bool checkSemantics(const std::string& sqlQuery);
	bool checkSecurity(const std::string& sqlQuery);
	bool checkComplexity(const std::string& sqlQuery);
	void resetValidationState();
};

} /* namespace FauxDB */
