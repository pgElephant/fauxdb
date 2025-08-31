/*-------------------------------------------------------------------------
 *
 * CMongoWireProtocol.cpp
 *      Document Wire Protocol implementation for FauxDB.
 *      Based on RFC-FauxDB-002 Document Wire Protocol specification.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#include "CDocumentWireProtocol.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
using namespace std;
using namespace FauxDB;
// FauxDB namespace removed for direct symbol usage

/*-------------------------------------------------------------------------
 * CDocumentMessageHeader implementation
 *-------------------------------------------------------------------------*/

std::vector<uint8_t> CDocumentMessageHeader::serialize() const
{
	std::vector<uint8_t> result;
	result.resize(16);

	/* Little-endian serialization */
	std::memcpy(result.data(), &messageLength, sizeof(int32_t));
	std::memcpy(result.data() + 4, &requestID, sizeof(int32_t));
	std::memcpy(result.data() + 8, &responseTo, sizeof(int32_t));
	std::memcpy(result.data() + 12, &opCode, sizeof(int32_t));

	return result;
}

bool CDocumentMessageHeader::deserialize(const std::vector<uint8_t>& data)
{
	if (data.size() < 16)
	{
		return false;
	}

	/* Little-endian deserialization */
	std::memcpy(&messageLength, data.data(), sizeof(int32_t));
	std::memcpy(&requestID, data.data() + 4, sizeof(int32_t));
	std::memcpy(&responseTo, data.data() + 8, sizeof(int32_t));
	std::memcpy(&opCode, data.data() + 12, sizeof(int32_t));

	return true;
}

bool CDocumentMessageHeader::isValid() const
{
	return messageLength >= 16 && messageLength <= 48000000;
}

/*-------------------------------------------------------------------------
 * CDocumentMsgSection0 implementation
 *-------------------------------------------------------------------------*/

std::vector<uint8_t> CDocumentMsgSection0::serialize() const
{
	std::vector<uint8_t> result;
	result.push_back(kind);
	result.insert(result.end(), bsonDoc.begin(), bsonDoc.end());
	return result;
}

bool CDocumentMsgSection0::deserialize(const std::vector<uint8_t>& data,
									size_t& offset)
{
	if (offset >= data.size())
	{
		return false;
	}

	kind = data[offset++];
	if (kind != 0x00)
	{
		return false;
	}

	/* Read BSON document */
	if (offset + 4 > data.size())
	{
		return false;
	}

	int32_t docSize;
	std::memcpy(&docSize, data.data() + offset, sizeof(int32_t));

	if (docSize <= 0 || offset + docSize > data.size())
	{
		return false;
	}

	bsonDoc.assign(data.begin() + offset, data.begin() + offset + docSize);
	offset += docSize;

	return true;
}

size_t CDocumentMsgSection0::getSize() const
{
	return 1 + bsonDoc.size();
}

/*-------------------------------------------------------------------------
 * CDocumentMsgSection1 implementation
 *-------------------------------------------------------------------------*/

std::vector<uint8_t> CDocumentMsgSection1::serialize() const
{
	std::vector<uint8_t> result;
	result.push_back(kind);

	/* Add identifier as cstring */
	result.insert(result.end(), identifier.begin(), identifier.end());
	result.push_back(0x00); /* Null terminator */

	/* Add size */
	result.resize(result.size() + 4);
	std::memcpy(result.data() + result.size() - 4, &size, sizeof(int32_t));

	/* Add documents */
	for (const auto& doc : documents)
	{
		result.insert(result.end(), doc.begin(), doc.end());
	}

	return result;
}

bool CDocumentMsgSection1::deserialize(const std::vector<uint8_t>& data,
									size_t& offset)
{
	if (offset >= data.size())
	{
		return false;
	}

	kind = data[offset++];
	if (kind != 0x01)
	{
		return false;
	}

	/* Read identifier as cstring */
	size_t start = offset;
	while (offset < data.size() && data[offset] != 0x00)
	{
		offset++;
	}

	if (offset >= data.size())
	{
		return false;
	}

	identifier = std::string(data.begin() + start, data.begin() + offset);
	offset++; /* Skip null terminator */

	/* Read size */
	if (offset + 4 > data.size())
	{
		return false;
	}

	std::memcpy(&size, data.data() + offset, sizeof(int32_t));
	offset += 4;

	/* Read documents */
	size_t endOffset = offset + size - 4; /* size includes the int32 itself */
	documents.clear();

	while (offset < endOffset && offset < data.size())
	{
		if (offset + 4 > data.size())
		{
			break;
		}

		int32_t docSize;
		std::memcpy(&docSize, data.data() + offset, sizeof(int32_t));

		if (docSize <= 0 || offset + docSize > data.size())
		{
			break;
		}

		std::vector<uint8_t> doc(data.begin() + offset,
								 data.begin() + offset + docSize);
		documents.push_back(doc);
		offset += docSize;
	}

	return true;
}

size_t CDocumentMsgSection1::getSize() const
{
	size_t totalSize =
		1 + identifier.length() + 1 + 4; /* kind + identifier + null + size */
	for (const auto& doc : documents)
	{
		totalSize += doc.size();
	}
	return totalSize;
}

/*-------------------------------------------------------------------------
 * CDocumentMsgBody implementation
 *-------------------------------------------------------------------------*/

/* Copy constructor */
CDocumentMsgBody::CDocumentMsgBody(const CDocumentMsgBody& other)
	: flagBits(other.flagBits), checksum(other.checksum)
{
	// Deep copy sections0
	for (const auto& section : other.sections0)
	{
		sections0.push_back(std::make_unique<CDocumentMsgSection0>(*section));
	}

	// Deep copy sections1
	for (const auto& section : other.sections1)
	{
		sections1.push_back(std::make_unique<CDocumentMsgSection1>(*section));
	}
}

/* Copy assignment operator */
CDocumentMsgBody& CDocumentMsgBody::operator=(const CDocumentMsgBody& other)
{
	if (this != &other)
	{
		flagBits = other.flagBits;
		checksum = other.checksum;

		// Clear existing sections
		sections0.clear();
		sections1.clear();

		// Deep copy sections0
		for (const auto& section : other.sections0)
		{
			sections0.push_back(std::make_unique<CDocumentMsgSection0>(*section));
		}

		// Deep copy sections1
		for (const auto& section : other.sections1)
		{
			sections1.push_back(std::make_unique<CDocumentMsgSection1>(*section));
		}
	}
	return *this;
}

/* Move constructor */
CDocumentMsgBody::CDocumentMsgBody(CDocumentMsgBody&& other) noexcept
	: flagBits(other.flagBits), checksum(other.checksum)
{
	sections0 = std::move(other.sections0);
	sections1 = std::move(other.sections1);

	// Reset other
	other.flagBits = 0;
	other.checksum = 0;
}

/* Move assignment operator */
CDocumentMsgBody& CDocumentMsgBody::operator=(CDocumentMsgBody&& other) noexcept
{
	if (this != &other)
	{
		flagBits = other.flagBits;
		checksum = other.checksum;

		sections0 = std::move(other.sections0);
		sections1 = std::move(other.sections1);

		// Reset other
		other.flagBits = 0;
		other.checksum = 0;
	}
	return *this;
}

std::vector<uint8_t> CDocumentMsgBody::serialize() const
{
	std::vector<uint8_t> result;

	/* Add flagBits */
	result.resize(4);
	std::memcpy(result.data(), &flagBits, sizeof(int32_t));

	/* Add sections */
	for (const auto& section : sections0)
	{
		auto sectionData = section->serialize();
		result.insert(result.end(), sectionData.begin(), sectionData.end());
	}

	for (const auto& section : sections1)
	{
		auto sectionData = section->serialize();
		result.insert(result.end(), sectionData.begin(), sectionData.end());
	}

	/* Add checksum if present */
	if (flagBits & static_cast<int32_t>(CDocumentMsgFlags::CHECKSUM_PRESENT))
	{
		result.resize(result.size() + 4);
		std::memcpy(result.data() + result.size() - 4, &checksum,
					sizeof(uint32_t));
	}

	return result;
}

bool CDocumentMsgBody::deserialize(const std::vector<uint8_t>& data,
								size_t& offset)
{
	if (offset + 4 > data.size())
	{
		return false;
	}

	/* Read flagBits */
	std::memcpy(&flagBits, data.data() + offset, sizeof(int32_t));
	offset += 4;

	/* Read sections */
	sections0.clear();
	sections1.clear();

	while (offset < data.size())
	{
		if (offset >= data.size())
		{
			break;
		}

		uint8_t kind = data[offset];

		if (kind == 0x00)
		{
			/* Section 0 */
			auto section = std::make_unique<CDocumentMsgSection0>();
			if (!section->deserialize(data, offset))
			{
				return false;
			}
			sections0.push_back(std::move(section));
		}
		else if (kind == 0x01)
		{
			/* Section 1 */
			auto section = std::make_unique<CDocumentMsgSection1>();
			if (!section->deserialize(data, offset))
			{
				return false;
			}
			sections1.push_back(std::move(section));
		}
		else
		{
			/* Unknown section kind */
			break;
		}
	}

	/* Read checksum if present */
	if (flagBits & static_cast<int32_t>(CDocumentMsgFlags::CHECKSUM_PRESENT))
	{
		if (offset + 4 > data.size())
		{
			return false;
		}
		std::memcpy(&checksum, data.data() + offset, sizeof(uint32_t));
		offset += 4;
	}

	return true;
}

size_t CDocumentMsgBody::getSize() const
{
	size_t totalSize = 4; /* flagBits */

	for (const auto& section : sections0)
	{
		totalSize += section->getSize();
	}

	for (const auto& section : sections1)
	{
		totalSize += section->getSize();
	}

	if (flagBits & static_cast<int32_t>(CDocumentMsgFlags::CHECKSUM_PRESENT))
	{
		totalSize += 4; /* checksum */
	}

	return totalSize;
}

bool CDocumentMsgBody::isValid() const
{
	/* Must have at least one section 0 */
	if (sections0.empty())
	{
		return false;
	}

	return true;
}

void CDocumentMsgBody::addSection0(std::unique_ptr<CDocumentMsgSection0> section)
{
	sections0.push_back(std::move(section));
}

void CDocumentMsgBody::addSection1(std::unique_ptr<CDocumentMsgSection1> section)
{
	sections1.push_back(std::move(section));
}

void CDocumentMsgBody::setChecksumPresent(bool present)
{
	if (present)
	{
		flagBits |= static_cast<int32_t>(CDocumentMsgFlags::CHECKSUM_PRESENT);
	}
	else
	{
		flagBits &= ~static_cast<int32_t>(CDocumentMsgFlags::CHECKSUM_PRESENT);
	}
}

void CDocumentMsgBody::computeChecksum()
{
	/* TODO: Implement CRC32C computation */
	checksum = 0;
}

bool CDocumentMsgBody::validateChecksum() const
{
	/* TODO: Implement CRC32C validation */
	return true;
}

/*-------------------------------------------------------------------------
 * CDocumentCompressedBody implementation
 *-------------------------------------------------------------------------*/

std::vector<uint8_t> CDocumentCompressedBody::serialize() const
{
	std::vector<uint8_t> result;

	/* Add originalOpcode */
	result.resize(4);
	std::memcpy(result.data(), &originalOpcode, sizeof(int32_t));

	/* Add uncompressedSize */
	result.resize(result.size() + 4);
	std::memcpy(result.data() + 4, &uncompressedSize, sizeof(int32_t));

	/* Add compressorId */
	result.push_back(compressorId);

	/* Add compressedBody */
	result.insert(result.end(), compressedBody.begin(), compressedBody.end());

	return result;
}

bool CDocumentCompressedBody::deserialize(const std::vector<uint8_t>& data,
									   size_t& offset)
{
	if (offset + 9 > data.size())
	{
		return false;
	}

	/* Read originalOpcode */
	std::memcpy(&originalOpcode, data.data() + offset, sizeof(int32_t));
	offset += 4;

	/* Read uncompressedSize */
	std::memcpy(&uncompressedSize, data.data() + offset, sizeof(int32_t));
	offset += 4;

	/* Read compressorId */
	compressorId = data[offset++];

	/* Read compressedBody */
	compressedBody.assign(data.begin() + offset, data.end());
	offset = data.size();

	return true;
}

size_t CDocumentCompressedBody::getSize() const
{
	return 4 + 4 + 1 + compressedBody.size();
}

bool CDocumentCompressedBody::isValid() const
{
	return compressorId >= 1 && compressorId <= 3;
}

/*-------------------------------------------------------------------------
 * CDocumentReplyBody implementation
 *-------------------------------------------------------------------------*/

std::vector<uint8_t> CDocumentReplyBody::serialize() const
{
	std::vector<uint8_t> result;

	/* Add responseFlags */
	result.resize(4);
	std::memcpy(result.data(), &responseFlags, sizeof(int32_t));

	/* Add cursorID */
	result.resize(result.size() + 8);
	std::memcpy(result.data() + 4, &cursorID, sizeof(int64_t));

	/* Add startingFrom */
	result.resize(result.size() + 4);
	std::memcpy(result.data() + 12, &startingFrom, sizeof(int32_t));

	/* Add numberReturned */
	result.resize(result.size() + 4);
	std::memcpy(result.data() + 16, &numberReturned, sizeof(int32_t));

	/* Add document */
	result.insert(result.end(), document.begin(), document.end());

	return result;
}

bool CDocumentReplyBody::deserialize(const std::vector<uint8_t>& data,
								  size_t& offset)
{
	if (offset + 20 > data.size())
	{
		return false;
	}

	/* Read responseFlags */
	std::memcpy(&responseFlags, data.data() + offset, sizeof(int32_t));
	offset += 4;

	/* Read cursorID */
	std::memcpy(&cursorID, data.data() + offset, sizeof(int64_t));
	offset += 8;

	/* Read startingFrom */
	std::memcpy(&startingFrom, data.data() + offset, sizeof(int32_t));
	offset += 4;

	/* Read numberReturned */
	std::memcpy(&numberReturned, data.data() + offset, sizeof(int32_t));
	offset += 4;

	/* Read document */
	document.assign(data.begin() + offset, data.end());
	offset = data.size();

	return true;
}

size_t CDocumentReplyBody::getSize() const
{
	return 4 + 8 + 4 + 4 + document.size();
}

bool CDocumentReplyBody::isValid() const
{
	return numberReturned == 1;
}

/*-------------------------------------------------------------------------
 * CDocumentWireMessage implementation
 *-------------------------------------------------------------------------*/

CDocumentWireMessage::CDocumentWireMessage()
{
}

CDocumentWireMessage::~CDocumentWireMessage()
{
}

void CDocumentWireMessage::setHeader(const CDocumentMessageHeader& header)
{
	header_ = header;
}

void CDocumentWireMessage::setMsgBody(const CDocumentMsgBody& body)
{
	msgBody_ = std::make_unique<CDocumentMsgBody>(body);
	updateMessageLength();
}

void CDocumentWireMessage::setCompressedBody(const CDocumentCompressedBody& body)
{
	compressedBody_ = std::make_unique<CDocumentCompressedBody>(body);
	updateMessageLength();
}

void CDocumentWireMessage::setReplyBody(const CDocumentReplyBody& body)
{
	replyBody_ = std::make_unique<CDocumentReplyBody>(body);
	updateMessageLength();
}

bool CDocumentWireMessage::parseFromBytes(const std::vector<uint8_t>& data)
{
	if (data.size() < 16)
	{
		return false;
	}

	/* Parse header */
	if (!header_.deserialize(data))
	{
		return false;
	}

	/* Parse body based on opCode */
	size_t offset = 16;

	// TODO: Implement body parsing when body classes are defined
	// For now, just skip body parsing to allow basic initialization
	
	switch (static_cast<CDocumentOpCode>(header_.opCode))
	{
	case CDocumentOpCode::OP_MSG:
	{
		msgBody_ = std::make_unique<CDocumentMsgBody>();
		if (!msgBody_->deserialize(data, offset))
		{
			return false;
		}
		break;
	}
	case CDocumentOpCode::OP_COMPRESSED:
	{
		compressedBody_ = std::make_unique<CDocumentCompressedBody>();
		if (!compressedBody_->deserialize(data, offset))
		{
			return false;
		}
		break;
	}
	case CDocumentOpCode::OP_REPLY:
	{
		replyBody_ = std::make_unique<CDocumentReplyBody>();
		if (!replyBody_->deserialize(data, offset))
		{
			return false;
		}
		break;
	}
	default:
		return false;
	}

	return true;
}

std::vector<uint8_t> CDocumentWireMessage::serializeToBytes() const
{
	std::vector<uint8_t> result;

	/* Add header */
	auto headerData = header_.serialize();
	result.insert(result.end(), headerData.begin(), headerData.end());

	/* Add body */
	// TODO: Implement body serialization when body classes are defined
	
	if (msgBody_)
	{
		auto bodyData = msgBody_->serialize();
		result.insert(result.end(), bodyData.begin(), bodyData.end());
	}
	else if (compressedBody_)
	{
		auto bodyData = compressedBody_->serialize();
		result.insert(result.end(), bodyData.begin(), bodyData.end());
	}
	else if (replyBody_)
	{
		auto bodyData = replyBody_->serialize();
		result.insert(result.end(), bodyData.begin(), bodyData.end());
	}


	return result;
}

bool CDocumentWireMessage::isValid() const
{
	return header_.isValid() && validateMessageStructure();
}

bool CDocumentWireMessage::isCompressed() const
{
	return compressedBody_ != nullptr;
}

bool CDocumentWireMessage::isOpMsg() const
{
	return msgBody_ != nullptr;
}

bool CDocumentWireMessage::isOpReply() const
{
	return replyBody_ != nullptr;
}

size_t CDocumentWireMessage::getTotalSize() const
{
	return header_.messageLength;
}

std::unique_ptr<CDocumentWireMessage> CDocumentWireMessage::createResponse() const
{
	auto response = std::make_unique<CDocumentWireMessage>();

	CDocumentMessageHeader responseHeader;
	responseHeader.requestID = 0; /* Will be set by caller */
	responseHeader.responseTo = header_.requestID;
	responseHeader.opCode = header_.opCode;

	response->setHeader(responseHeader);
	return response;
}

/*-------------------------------------------------------------------------
 * Static factory methods for common responses
 *-------------------------------------------------------------------------*/

std::unique_ptr<CDocumentWireMessage>
CDocumentWireMessage::createHelloResponse(int32_t requestID)
{
	auto response = std::make_unique<CDocumentWireMessage>();

	CDocumentMessageHeader header;
	header.requestID = 0; /* Will be set by caller */
	header.responseTo = requestID;
	header.opCode = static_cast<int32_t>(CDocumentOpCode::OP_MSG);

	response->setHeader(header);

	/* TODO: Create hello response BSON document */
	CDocumentMsgBody body;
	response->setMsgBody(body);

	return response;
}

std::unique_ptr<CDocumentWireMessage>
CDocumentWireMessage::createBuildInfoResponse(int32_t requestID)
{
	auto response = std::make_unique<CDocumentWireMessage>();

	CDocumentMessageHeader header;
	header.requestID = 0; /* Will be set by caller */
	header.responseTo = requestID;
	header.opCode = static_cast<int32_t>(CDocumentOpCode::OP_MSG);

	response->setHeader(header);

	/* TODO: Create buildInfo response BSON document */
	CDocumentMsgBody body;
	response->setMsgBody(body);

	return response;
}

std::unique_ptr<CDocumentWireMessage>
CDocumentWireMessage::createIsMasterResponse(int32_t requestID)
{
	auto response = std::make_unique<CDocumentWireMessage>();

	CDocumentMessageHeader header;
	header.requestID = 0; /* Will be set by caller */
	header.responseTo = requestID;
	header.opCode = static_cast<int32_t>(CDocumentOpCode::OP_REPLY);

	response->setHeader(header);

	/* TODO: Create isMaster response BSON document */
	CDocumentReplyBody body;
	response->setReplyBody(body);

	return response;
}

std::unique_ptr<CDocumentWireMessage> CDocumentWireMessage::createFindResponse(
	int32_t requestID, const std::vector<std::vector<uint8_t>>& documents)
{
	(void)documents; // Suppress unused parameter warning
	auto response = std::make_unique<CDocumentWireMessage>();

	CDocumentMessageHeader header;
	header.requestID = 0; /* Will be set by caller */
	header.responseTo = requestID;
	header.opCode = static_cast<int32_t>(CDocumentOpCode::OP_MSG);

	response->setHeader(header);

	/* TODO: Create find response BSON document */
	CDocumentMsgBody body;
	response->setMsgBody(body);

	return response;
}

std::unique_ptr<CDocumentWireMessage>
CDocumentWireMessage::createInsertResponse(int32_t requestID,
										int32_t insertedCount)
{
	(void)insertedCount; // Suppress unused parameter warning
	auto response = std::make_unique<CDocumentWireMessage>();

	CDocumentMessageHeader header;
	header.requestID = 0; /* Will be set by caller */
	header.responseTo = requestID;
	header.opCode = static_cast<int32_t>(CDocumentOpCode::OP_MSG);

	response->setHeader(header);

	/* TODO: Create insert response BSON document */
	CDocumentMsgBody body;
	response->setMsgBody(body);

	return response;
}

std::unique_ptr<CDocumentWireMessage>
CDocumentWireMessage::createUpdateResponse(int32_t requestID, int32_t matched,
										int32_t modified)
{
	(void)matched;	// Suppress unused parameter warning
	(void)modified; // Suppress unused parameter warning
	auto response = std::make_unique<CDocumentWireMessage>();

	CDocumentMessageHeader header;
	header.requestID = 0; /* Will be set by caller */
	header.responseTo = requestID;
	header.opCode = static_cast<int32_t>(CDocumentOpCode::OP_MSG);

	response->setHeader(header);

	/* TODO: Create update response BSON document */
	CDocumentMsgBody body;
	response->setMsgBody(body);

	return response;
}

std::unique_ptr<CDocumentWireMessage>
CDocumentWireMessage::createDeleteResponse(int32_t requestID, int32_t deletedCount)
{
	(void)deletedCount; // Suppress unused parameter warning
	auto response = std::make_unique<CDocumentWireMessage>();

	CDocumentMessageHeader header;
	header.requestID = 0; /* Will be set by caller */
	header.responseTo = requestID;
	header.opCode = static_cast<int32_t>(CDocumentOpCode::OP_MSG);

	response->setHeader(header);

	/* TODO: Create delete response BSON document */
	CDocumentMsgBody body;
	response->setMsgBody(body);

	return response;
}

void CDocumentWireMessage::updateMessageLength()
{
	size_t bodySize = 0;

	if (msgBody_)
	{
		bodySize = msgBody_->getSize();
	}
	else if (compressedBody_)
	{
		bodySize = compressedBody_->getSize();
	}
	else if (replyBody_)
	{
		bodySize = replyBody_->getSize();
	}

	header_.messageLength = 16 + bodySize;
}

bool CDocumentWireMessage::validateMessageStructure() const
{
	if (isOpMsg() && !msgBody_->isValid())
	{
		return false;
	}

	if (isCompressed() && !compressedBody_->isValid())
	{
		return false;
	}

	if (isOpReply() && !replyBody_->isValid())
	{
		return false;
	}

	return true;
}

/*-------------------------------------------------------------------------
 * CMongoWireParser implementation
 *-------------------------------------------------------------------------*/

CDocumentWireParser::CDocumentWireParser()
{
}

CDocumentWireParser::~CDocumentWireParser()
{
}

std::unique_ptr<CDocumentWireMessage>
CDocumentWireParser::parseMessage(const std::vector<uint8_t>& data)
{
	if (!validateMessageSize(data.size()))
	{
		return nullptr;
	}

	auto message = std::make_unique<CDocumentWireMessage>();
	if (!message->parseFromBytes(data))
	{
		return nullptr;
	}

	return message;
}

bool CDocumentWireParser::parseHeader(const std::vector<uint8_t>& data,
								   CDocumentMessageHeader& header)
{
	if (data.size() < HEADER_SIZE)
	{
		return false;
	}

	return header.deserialize(data);
}

bool CDocumentWireParser::parseMsgBody(const std::vector<uint8_t>& data,
									size_t& offset, CDocumentMsgBody& body)
{
	return body.deserialize(data, offset);
}

bool CDocumentWireParser::parseCompressedBody(const std::vector<uint8_t>& data,
										   size_t& offset,
										   CDocumentCompressedBody& body)
{
	return body.deserialize(data, offset);
}

bool CDocumentWireParser::parseReplyBody(const std::vector<uint8_t>& data,
									  size_t& offset, CDocumentReplyBody& body)
{
	return body.deserialize(data, offset);
}

bool CDocumentWireParser::parseBsonDocument(const std::vector<uint8_t>& data,
										 size_t& offset,
										 std::vector<uint8_t>& document)
{
	if (offset + 4 > data.size())
	{
		return false;
	}

	int32_t docSize;
	std::memcpy(&docSize, data.data() + offset, sizeof(int32_t));

	if (docSize <= 0 || !validateBsonSize(docSize) ||
		offset + docSize > data.size())
	{
		return false;
	}

	document.assign(data.begin() + offset, data.begin() + offset + docSize);
	offset += docSize;

	return true;
}

bool CDocumentWireParser::parseCString(const std::vector<uint8_t>& data,
									size_t& offset, std::string& result)
{
	size_t start = offset;
	while (offset < data.size() && data[offset] != 0x00)
	{
		offset++;
	}

	if (offset >= data.size())
	{
		return false;
	}

	result = std::string(data.begin() + start, data.begin() + offset);
	offset++; /* Skip null terminator */

	return true;
}

bool CDocumentWireParser::parseInt32(const std::vector<uint8_t>& data,
								  size_t& offset, int32_t& result)
{
	if (offset + 4 > data.size())
	{
		return false;
	}

	std::memcpy(&result, data.data() + offset, sizeof(int32_t));
	offset += 4;

	return true;
}

bool CDocumentWireParser::parseInt64(const std::vector<uint8_t>& data,
								  size_t& offset, int64_t& result)
{
	if (offset + 8 > data.size())
	{
		return false;
	}

	std::memcpy(&result, data.data() + offset, sizeof(int64_t));
	offset += 8;

	return true;
}

bool CDocumentWireParser::parseUint8(const std::vector<uint8_t>& data,
								  size_t& offset, uint8_t& result)
{
	if (offset >= data.size())
	{
		return false;
	}

	result = data[offset++];
	return true;
}

bool CDocumentWireParser::parseUint32(const std::vector<uint8_t>& data,
								   size_t& offset, uint32_t& result)
{
	if (offset + 4 > data.size())
	{
		return false;
	}

	std::memcpy(&result, data.data() + offset, sizeof(uint32_t));
	offset += 4;

	return true;
}

bool CDocumentWireParser::validateMessageSize(size_t size) const
{
	return size >= HEADER_SIZE && size <= MAX_MESSAGE_SIZE;
}

bool CDocumentWireParser::validateBsonSize(size_t size) const
{
	return size <= MAX_BSON_SIZE;
}

uint32_t CDocumentWireParser::computeCRC32C(const std::vector<uint8_t>& data,
										 size_t start, size_t length) const
{
	(void)data;	  // Suppress unused parameter warning
	(void)start;  // Suppress unused parameter warning
	(void)length; // Suppress unused parameter warning
	/* TODO: Implement CRC32C computation using Castagnoli polynomial 0x1EDC6F41
	 */
	return 0;
}

// End of file
