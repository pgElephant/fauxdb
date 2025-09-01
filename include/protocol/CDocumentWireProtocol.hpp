/*-------------------------------------------------------------------------
 *
 * CDocumentWireProtocol.hpp
 *      Document Wire Protocol implementation for FauxDB.
 *      Based on RFC-FauxDB-002 Document Wire Protocol specification.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
using namespace std;
namespace FauxDB
{

using namespace FauxDB;

/**
 * Document Wire Protocol OpCodes
 */
enum class CDocumentOpCode : int32_t
{
    OP_REPLY = 1,
    OP_MSG = 2013,
    OP_COMPRESSED = 2012,
    OP_QUERY = 2004,
    OP_GET_MORE = 2005,
    OP_DELETE = 2006,
    OP_KILL_CURSORS = 2007,
    OP_INSERT = 2002,
    OP_UPDATE = 2001
};

/**
 * Document Wire Protocol Message Header (16 bytes)
 */
struct CDocumentMessageHeader
{
    int32_t messageLength; /* Total size including header */
    int32_t requestID;     /* Chosen by sender */
    int32_t responseTo;    /* For replies, echo requestID of request. Zero for
                              requests */
    int32_t opCode;        /* Operation code */

    CDocumentMessageHeader()
        : messageLength(0), requestID(0), responseTo(0), opCode(0)
    {
    }

    /* Serialize header to bytes */
    std::vector<uint8_t> serialize() const;

    /* Deserialize header from bytes */
    bool deserialize(const std::vector<uint8_t>& data);

    /* Validate header */
    bool isValid() const;
};

/**
 * OP_MSG Flag Bits
 */
enum class CDocumentMsgFlags : int32_t
{
    CHECKSUM_PRESENT = 0x00000001, /* Bit 0: If 1, append CRC32C at end */
    MORE_TO_COME = 0x00000002, /* Bit 1: If 1, do not expect client response */
    EXHAUST_ALLOWED = 0x00010000 /* Bit 16: Request side only */
};

/**
 * OP_MSG Section Kinds
 */
enum class CDocumentSectionKind : uint8_t
{
    KIND_0 = 0x00, /* One BSON document */
    KIND_1 = 0x01  /* Document sequence */
};

/**
 * OP_MSG Section (Kind 0)
 */
struct CDocumentMsgSection0
{
    uint8_t kind;                 /* Always 0x00 */
    std::vector<uint8_t> bsonDoc; /* BSON document */

    CDocumentMsgSection0() : kind(0x00)
    {
    }

    /* Serialize section to bytes */
    std::vector<uint8_t> serialize() const;

    /* Deserialize section from bytes */
    bool deserialize(const std::vector<uint8_t>& data, size_t& offset);

    /* Get section size */
    size_t getSize() const;
};

/**
 * OP_MSG Section (Kind 1)
 */
struct CDocumentMsgSection1
{
    uint8_t kind;           /* Always 0x01 */
    std::string identifier; /* cstring identifier */
    int32_t size;           /* Byte count that follows */
    std::vector<std::vector<uint8_t>>
        documents; /* Sequence of BSON documents */

    CDocumentMsgSection1() : kind(0x01), size(0)
    {
    }

    /* Serialize section to bytes */
    std::vector<uint8_t> serialize() const;

    /* Deserialize section from bytes */
    bool deserialize(const std::vector<uint8_t>& data, size_t& offset);

    /* Get section size */
    size_t getSize() const;
};

/**
 * OP_MSG Message Body
 */
struct CDocumentMsgBody
{
    int32_t flagBits; /* Flag bits */
    std::vector<std::unique_ptr<CDocumentMsgSection0>>
        sections0; /* Kind 0 sections */
    std::vector<std::unique_ptr<CDocumentMsgSection1>>
        sections1;     /* Kind 1 sections */
    uint32_t checksum; /* CRC32C if flagBits bit 0 is set */

    CDocumentMsgBody() : flagBits(0), checksum(0)
    {
    }

    /* Copy constructor */
    CDocumentMsgBody(const CDocumentMsgBody& other);

    /* Copy assignment operator */
    CDocumentMsgBody& operator=(const CDocumentMsgBody& other);

    /* Move constructor */
    CDocumentMsgBody(CDocumentMsgBody&& other) noexcept;

    /* Move assignment operator */
    CDocumentMsgBody& operator=(CDocumentMsgBody&& other) noexcept;

    /* Serialize body to bytes */
    std::vector<uint8_t> serialize() const;

    /* Deserialize body from bytes */
    bool deserialize(const std::vector<uint8_t>& data, size_t& offset);

    /* Get body size */
    size_t getSize() const;

    /* Validate body */
    bool isValid() const;

    /* Add section 0 */
    void addSection0(std::unique_ptr<CDocumentMsgSection0> section);

    /* Add section 1 */
    void addSection1(std::unique_ptr<CDocumentMsgSection1> section);

    /* Set checksum flag */
    void setChecksumPresent(bool present);

    /* Compute and set checksum */
    void computeChecksum();

    /* Validate checksum */
    bool validateChecksum() const;
};

/**
 * OP_COMPRESSED Message Body
 */
struct CDocumentCompressedBody
{
    int32_t originalOpcode;   /* Usually 2013 */
    int32_t uncompressedSize; /* Size of original uncompressed body */
    uint8_t compressorId;     /* 1=snappy, 2=zlib, 3=zstd */
    std::vector<uint8_t> compressedBody; /* Compressed original body */

    CDocumentCompressedBody()
        : originalOpcode(0), uncompressedSize(0), compressorId(0)
    {
    }

    /* Serialize body to bytes */
    std::vector<uint8_t> serialize() const;

    /* Deserialize body from bytes */
    bool deserialize(const std::vector<uint8_t>& data, size_t& offset);

    /* Get body size */
    size_t getSize() const;

    /* Validate body */
    bool isValid() const;
};

/**
 * OP_REPLY Message Body (Legacy)
 */
struct CDocumentReplyBody
{
    int32_t responseFlags;         /* Zero */
    int64_t cursorID;              /* Zero */
    int32_t startingFrom;          /* Zero */
    int32_t numberReturned;        /* Must be 1 */
    std::vector<uint8_t> document; /* One BSON document */

    CDocumentReplyBody()
        : responseFlags(0), cursorID(0), startingFrom(0), numberReturned(1)
    {
    }

    /* Serialize body to bytes */
    std::vector<uint8_t> serialize() const;

    /* Deserialize body from bytes */
    bool deserialize(const std::vector<uint8_t>& data, size_t& offset);

    /* Get body size */
    size_t getSize() const;

    /* Validate body */
    bool isValid() const;
};

/**
 * Complete Document Wire Protocol Message
 */
class CDocumentWireMessage
{
  public:
    CDocumentWireMessage();
    ~CDocumentWireMessage();
    // ...existing code...

    /* Message construction */
    void setHeader(const CDocumentMessageHeader& header);
    void setMsgBody(const CDocumentMsgBody& body);
    void setCompressedBody(const CDocumentCompressedBody& body);
    void setReplyBody(const CDocumentReplyBody& body);

    /* Message parsing */
    bool parseFromBytes(const std::vector<uint8_t>& data);
    std::vector<uint8_t> serializeToBytes() const;

    /* Message validation */
    bool isValid() const;
    bool isCompressed() const;
    bool isOpMsg() const;
    bool isOpReply() const;

    /* Getters */
    const CDocumentMessageHeader& getHeader() const
    {
        return header_;
    }
    const CDocumentMsgBody* getMsgBody() const
    {
        return msgBody_.get();
    }
    const CDocumentCompressedBody* getCompressedBody() const
    {
        return compressedBody_.get();
    }
    const CDocumentReplyBody* getReplyBody() const
    {
        return replyBody_.get();
    }

    /* Message size */
    size_t getTotalSize() const;

    /* Create response message */
    std::unique_ptr<CDocumentWireMessage> createResponse() const;

    /* Create hello response */
    static std::unique_ptr<CDocumentWireMessage>
    createHelloResponse(int32_t requestID);

    /* Create buildInfo response */
    static std::unique_ptr<CDocumentWireMessage>
    createBuildInfoResponse(int32_t requestID);

    /* Create isMaster response */
    static std::unique_ptr<CDocumentWireMessage>
    createIsMasterResponse(int32_t requestID);

    /* Create find response */
    static std::unique_ptr<CDocumentWireMessage>
    createFindResponse(int32_t requestID,
                       const std::vector<std::vector<uint8_t>>& documents);

    /* Create insert response */
    static std::unique_ptr<CDocumentWireMessage>
    createInsertResponse(int32_t requestID, int32_t insertedCount);

    /* Create update response */
    static std::unique_ptr<CDocumentWireMessage>
    createUpdateResponse(int32_t requestID, int32_t matched, int32_t modified);

    /* Create delete response */
    static std::unique_ptr<CDocumentWireMessage>
    createDeleteResponse(int32_t requestID, int32_t deletedCount);

  private:
    CDocumentMessageHeader header_;
    std::unique_ptr<CDocumentMsgBody> msgBody_;
    std::unique_ptr<CDocumentCompressedBody> compressedBody_;
    std::unique_ptr<CDocumentReplyBody> replyBody_;

    /* Private helper methods */
    void updateMessageLength();
    bool validateMessageStructure() const;
};

/**
 * Document Wire Protocol Parser
 */
class CDocumentWireParser
{
  public:
    CDocumentWireParser();
    ~CDocumentWireParser();

    /* Parse message from bytes */
    std::unique_ptr<CDocumentWireMessage>
    parseMessage(const std::vector<uint8_t>& data);

    /* Parse message header */
    bool parseHeader(const std::vector<uint8_t>& data,
                     CDocumentMessageHeader& header);

    /* Parse OP_MSG body */
    bool parseMsgBody(const std::vector<uint8_t>& data, size_t& offset,
                      CDocumentMsgBody& body);

    /* Parse OP_COMPRESSED body */
    bool parseCompressedBody(const std::vector<uint8_t>& data, size_t& offset,
                             CDocumentCompressedBody& body);

    /* Parse OP_REPLY body */
    bool parseReplyBody(const std::vector<uint8_t>& data, size_t& offset,
                        CDocumentReplyBody& body);

    /* Parse BSON document */
    bool parseBsonDocument(const std::vector<uint8_t>& data, size_t& offset,
                           std::vector<uint8_t>& document);

    /* Parse cstring */
    bool parseCString(const std::vector<uint8_t>& data, size_t& offset,
                      std::string& result);

    /* Parse int32 (little-endian) */
    bool parseInt32(const std::vector<uint8_t>& data, size_t& offset,
                    int32_t& result);

    /* Parse int64 (little-endian) */
    bool parseInt64(const std::vector<uint8_t>& data, size_t& offset,
                    int64_t& result);

    /* Parse uint8 */
    bool parseUint8(const std::vector<uint8_t>& data, size_t& offset,
                    uint8_t& result);

    /* Parse uint32 (little-endian) */
    bool parseUint32(const std::vector<uint8_t>& data, size_t& offset,
                     uint32_t& result);

  private:
    /* Validation constants */
    static constexpr size_t MAX_MESSAGE_SIZE = 48000000; /* 48MB */
    static constexpr size_t MAX_BSON_SIZE = 16777216;    /* 16MB */
    static constexpr size_t HEADER_SIZE = 16;

    /* Private helper methods */
    bool validateMessageSize(size_t size) const;
    bool validateBsonSize(size_t size) const;
    uint32_t computeCRC32C(const std::vector<uint8_t>& data, size_t start,
                           size_t length) const;
};

} /* namespace FauxDB */
