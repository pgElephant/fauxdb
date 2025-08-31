/*-------------------------------------------------------------------------
 *
 * CDocumentQueryParser.hpp
 *      Document document query parser for FauxDB.
 *      Parses Document wire protocol queries and BSON documents.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#pragma once
#include "CParser.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
namespace FauxDB
{

using namespace FauxDB;

/**
 * Document wire protocol message types
 */
enum class CDocumentMessageType : uint8_t
{
	OP_REPLY = 1,
	OP_MSG = 1000,
	OP_UPDATE = 2001,
	OP_INSERT = 2002,
	OP_QUERY = 2004,
	OP_GET_MORE = 2005,
	OP_DELETE = 2006,
	OP_KILL_CURSORS = 2007,
	OP_COMPRESSED = 2012,
	OP_MSG_LEGACY = 2013
};

/**
 * Document query operator types
 */
enum class CDocumentOperator : uint8_t
{
	Equal = 0,
	NotEqual = 1,
	GreaterThan = 2,
	GreaterThanEqual = 3,
	LessThan = 4,
	LessThanEqual = 5,
	In = 6,
	NotIn = 7,
	Exists = 8,
	Type = 9,
	Regex = 10,
	Text = 11,
	GeoWithin = 12,
	GeoIntersects = 13,
	Near = 14,
	NearSphere = 15,
	All = 16,
	ElemMatch = 17,
	Size = 18,
	Mod = 19
};

/**
 * Document document query parser
 * Parses Document wire protocol queries and BSON documents
 */
class CDocumentQueryParser : public CParser
{
  public:
	CDocumentQueryParser();
	virtual ~CDocumentQueryParser();
// ...existing code...

	/* CParser interface implementation */
	CParserResult parse(const std::string& input) override;
	CParserResult parse(const std::vector<uint8_t>& input) override;
	CParserResult parse(const uint8_t* input, size_t length) override;

	bool validateInput(const std::string& input) override;
	bool validateInput(const std::vector<uint8_t>& input) override;
	bool validateInput(const uint8_t* input, size_t length) override;

	size_t estimateOutputSize(const std::string& input) const override;
	size_t estimateOutputSize(const std::vector<uint8_t>& input) const override;
	std::string getParserInfo() const override;

	/* Document-specific parsing */
	CParserResult parseWireMessage(const std::vector<uint8_t>& message);
	CParserResult parseBSONDocument(const std::vector<uint8_t>& bsonData);
	CParserResult parseQueryDocument(const std::vector<uint8_t>& queryData);
	CParserResult parseUpdateDocument(const std::vector<uint8_t>& updateData);
	CParserResult parseInsertDocument(const std::vector<uint8_t>& insertData);

	/* Document message parsing */
	bool parseMessageHeader(const std::vector<uint8_t>& header);
	bool parseMessageBody(const std::vector<uint8_t>& body);
	bool parseQuerySection(const std::vector<uint8_t>& querySection);
	bool parseProjectionSection(const std::vector<uint8_t>& projectionSection);
	bool parseSortSection(const std::vector<uint8_t>& sortSection);

	/* BSON parsing */
	bool parseBSONElement(const std::vector<uint8_t>& elementData,
						  size_t& offset);
	bool parseBSONString(const std::vector<uint8_t>& stringData,
						 size_t& offset);
	bool parseBSONNumber(const std::vector<uint8_t>& numberData,
						 size_t& offset);
	bool parseBSONObject(const std::vector<uint8_t>& objectData,
						 size_t& offset);
	bool parseBSONArray(const std::vector<uint8_t>& arrayData, size_t& offset);

	/* Query operator parsing */
	bool parseQueryOperators(const std::vector<uint8_t>& operatorData);
	bool parseComparisonOperator(CDocumentOperator op,
								 const std::vector<uint8_t>& valueData);
	bool parseLogicalOperator(const std::string& logicalOp,
							  const std::vector<uint8_t>& operandsData);
	bool parseArrayOperator(CDocumentOperator op,
							const std::vector<uint8_t>& arrayData);

	/* Document query analysis */
	std::vector<std::string>
	getQueryFields(const std::vector<uint8_t>& queryData);
	std::vector<CDocumentOperator>
	getQueryOperators(const std::vector<uint8_t>& queryData);
	std::string getCollectionName(const std::vector<uint8_t>& queryData);
	std::string getDatabaseName(const std::vector<uint8_t>& queryData);

	/* Document query validation */
	bool validateWireMessage(const std::vector<uint8_t>& message);
	bool validateBSONDocument(const std::vector<uint8_t>& bsonData);
	bool validateQueryStructure(const std::vector<uint8_t>& queryData);
	bool validateUpdateStructure(const std::vector<uint8_t>& updateData);

	/* Document query optimization */
	void setQueryOptimization(bool enabled);
	void setIndexHints(const std::vector<std::string>& hints);
	void setReadPreference(const std::string& preference);
	void setWriteConcern(const std::string& concern);

  protected:
	/* CParser interface implementation */
	void reset() override;
	void clear() override;

	/* Protected utility methods */
	virtual void setError(const std::string& error,
						  CParserStatus status) override;
	virtual bool checkBufferSize(size_t requiredSize) override;
	virtual void resizeBuffer(size_t newSize) override;
	virtual bool validateBuffer() override;

  private:
	/* Document parsing state */
	std::string currentDatabase_;
	std::string currentCollection_;
	CDocumentMessageType currentMessageType_;
	uint32_t currentMessageLength_;
	uint32_t currentRequestId_;
	uint32_t currentResponseTo_;

	/* Query parsing state */
	std::unordered_map<std::string, std::string> parsedFields_;
	std::vector<CDocumentOperator> parsedOperators_;
	std::vector<std::string> parsedValues_;
	std::string queryPlan_;

	/* BSON parsing state */
	std::vector<std::string> bsonFieldNames_;
	std::vector<std::string> bsonFieldValues_;
	std::vector<uint8_t> bsonFieldTypes_;

	/* Query optimization state */
	bool queryOptimizationEnabled_;
	std::vector<std::string> indexHints_;
	std::string readPreference_;
	std::string writeConcern_;

	/* Private methods */
	bool parseDocumentQuery(const std::vector<uint8_t>& queryData);
	bool parseQueryFilter(const std::vector<uint8_t>& filterData);
	bool parseQueryProjection(const std::vector<uint8_t>& projectionData);
	bool parseQuerySort(const std::vector<uint8_t>& sortData);

	/* BSON parsing helpers */
	uint8_t getBSONElementType(const std::vector<uint8_t>& elementData,
							   size_t offset);
	std::string getBSONElementName(const std::vector<uint8_t>& elementData,
								   size_t& offset);
	size_t getBSONElementSize(const std::vector<uint8_t>& elementData,
							  size_t offset);
	bool skipBSONElement(const std::vector<uint8_t>& elementData,
						 size_t& offset);

	/* Query parsing helpers */
	std::string extractFieldName(const std::vector<uint8_t>& fieldData);
	std::string extractFieldValue(const std::vector<uint8_t>& valueData);
	CDocumentOperator extractOperator(const std::string& operatorStr);
	std::string buildQueryExpression(const std::string& field,
									 CDocumentOperator op,
									 const std::string& value);

	/* Validation helpers */
	bool validateBSONStructure(const std::vector<uint8_t>& bsonData);
	bool validateQuerySyntax(const std::vector<uint8_t>& queryData);
	bool validateFieldNames(const std::vector<std::string>& fields);
	bool validateOperatorUsage(const std::vector<CDocumentOperator>& operators);

	/* Utility methods */
	std::string operatorToString(CDocumentOperator op) const;
	CDocumentOperator stringToOperator(const std::string& opStr) const;
	std::string buildErrorMessage(const std::string& operation,
								  const std::string& details);
	void resetQueryState();
	void clearParsedData();

	/* Query optimization */
	std::string optimizeQueryPlan(const std::vector<uint8_t>& queryData);
	std::vector<std::string>
	suggestIndexes(const std::vector<uint8_t>& queryData);
	bool canUseIndex(const std::string& field, CDocumentOperator op);
};

} /* namespace FauxDB */
