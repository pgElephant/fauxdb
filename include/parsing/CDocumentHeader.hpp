/*-------------------------------------------------------------------------
 *
 * CDocumentHeader.hpp
 *      Document header structure for FauxDB.
 *      Represents the header information of a parsed document.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>
using namespace std;
namespace FauxDB
{

using namespace FauxDB;

/**
 * Document header structure
 * Contains metadata about a parsed document
 */
struct CDocumentHeader
{
	/* Document identification */
	std::string documentType;
// ...existing code...
	std::string collectionName;
	std::string databaseName;

	/* Document metadata */
	uint32_t documentId;
	uint32_t version;
	uint64_t timestamp;

	/* Document wire protocol specific fields */
	uint32_t messageLength;
	uint32_t requestID;
	uint32_t responseTo;
	uint32_t opCode;

	/* Document size information */
	size_t headerSize;
	size_t bodySize;
	size_t totalSize;

	/* Document flags */
	bool isCompressed;
	bool isEncrypted;
	bool isBatch;

	/* Constructor */
	CDocumentHeader()
		: documentId(0), version(1), timestamp(0), messageLength(0),
		  requestID(0), responseTo(0), opCode(0), headerSize(0), bodySize(0),
		  totalSize(0), isCompressed(false), isEncrypted(false), isBatch(false)
	{
	}

	/* Reset method */
	void reset()
	{
		documentType.clear();
		collectionName.clear();
		databaseName.clear();
		documentId = 0;
		version = 1;
		timestamp = 0;
		messageLength = 0;
		requestID = 0;
		responseTo = 0;
		opCode = 0;
		headerSize = 0;
		bodySize = 0;
		totalSize = 0;
		isCompressed = false;
		isEncrypted = false;
		isBatch = false;
	}

	/* Validation */
	bool isValid() const
	{
		return !documentType.empty() && totalSize > 0;
	}
};

} /* namespace FauxDB */
