

#include <cstring>
#include <stdexcept>




#include "CMessageParser.hpp"

namespace FauxDB {

CMessageParser::CMessageParser()
    : messageValid_(false), errorMessage_(""), parsedRequestId_(0), parsedResponseTo_(0) {
}

CMessageParser::~CMessageParser() {
}

std::error_code CMessageParser::parseMessage(const std::vector<uint8_t>& rawMessage) {
    reset();

    if (rawMessage.size() < 16) {
        errorMessage_ = "Message too short";
        return std::make_error_code(std::errc::message_size);
    }

    std::error_code ec = parseMessageHeader(rawMessage);
    if (ec) {
        return ec;
    }

    ec = parseMessageBody(rawMessage);
    if (ec) {
        return ec;
    }

    messageValid_ = true;
    return std::error_code();
}

bool CMessageParser::isValidMessage() const noexcept {
    return messageValid_;
}

std::string CMessageParser::getErrorMessage() const {
    return errorMessage_;
}

void CMessageParser::reset() noexcept {
    messageValid_ = false;
    errorMessage_.clear();
    parsedCollection_.clear();
    parsedDatabase_.clear();
    parsedQuery_.clear();
    parsedDocument_.clear();
    parsedRequestId_ = 0;
    parsedResponseTo_ = 0;
}

CDocumentHeader CMessageParser::getParsedHeader() const {
    return parsedHeader_;
}

std::string CMessageParser::getParsedCollection() const {
    return parsedCollection_;
}

std::string CMessageParser::getParsedDatabase() const {
    return parsedDatabase_;
}

std::string CMessageParser::getParsedQuery() const {
    return parsedQuery_;
}

std::string CMessageParser::getParsedDocument() const {
    return parsedDocument_;
}

uint32_t CMessageParser::getParsedRequestId() const {
    return parsedRequestId_;
}

uint32_t CMessageParser::getParsedResponseTo() const {
    return parsedResponseTo_;
}

std::error_code CMessageParser::parseQueryMessage(const std::vector<uint8_t>& message) {
    if (message.size() < 16) {
        errorMessage_ = "Message too short for query parsing";
        return std::make_error_code(std::errc::message_size);
    }

    try {
        auto headerResult = parseMessageHeader(message);
        if (headerResult) return headerResult;

        auto bodyResult = parseMessageBody(message);
        if (bodyResult) return bodyResult;

        if (parsedQuery_.empty()) {
            errorMessage_ = "Empty query in message";
            return std::make_error_code(std::errc::invalid_argument);
        }

        return std::error_code();
    } catch (const std::exception& e) {
        errorMessage_ = "Exception during query parsing: " + std::string(e.what());
        return std::make_error_code(std::errc::invalid_argument);
    }
}

std::error_code CMessageParser::parseCommandMessage(const std::vector<uint8_t>& message) {
    if (message.size() < 16) {
        errorMessage_ = "Message too short for command parsing";
        return std::make_error_code(std::errc::message_size);
    }

    try {
        auto headerResult = parseMessageHeader(message);
        if (headerResult) return headerResult;

        auto bodyResult = parseMessageBody(message);
        if (bodyResult) return bodyResult;

        if (parsedQuery_.find("command") != std::string::npos) {
            parsedDocument_ = parsedQuery_;
        }

        return std::error_code();
    } catch (const std::exception& e) {
        errorMessage_ = "Exception during command parsing: " + std::string(e.what());
        return std::make_error_code(std::errc::invalid_argument);
    }
}

std::error_code CMessageParser::parseInsertMessage(const std::vector<uint8_t>& message) {
    if (message.size() < 16) {
        errorMessage_ = "Message too short for insert parsing";
        return std::make_error_code(std::errc::message_size);
    }

    try {
        auto headerResult = parseMessageHeader(message);
        if (headerResult) return headerResult;

        auto bodyResult = parseMessageBody(message);
        if (bodyResult) return bodyResult;

        parsedDocument_ = parsedQuery_;

        return std::error_code();
    } catch (const std::exception& e) {
        errorMessage_ = "Exception during insert parsing: " + std::string(e.what());
        return std::make_error_code(std::errc::invalid_argument);
    }
}

std::error_code CMessageParser::parseUpdateMessage(const std::vector<uint8_t>& message) {
    if (message.size() < 16) {
        errorMessage_ = "Message too short for update parsing";
        return std::make_error_code(std::errc::message_size);
    }

    try {
        auto headerResult = parseMessageHeader(message);
        if (headerResult) return headerResult;

        auto bodyResult = parseMessageBody(message);
        if (bodyResult) return bodyResult;

        parsedDocument_ = parsedQuery_;

        return std::error_code();
    } catch (const std::exception& e) {
        errorMessage_ = "Exception during update parsing: " + std::string(e.what());
        return std::make_error_code(std::errc::invalid_argument);
    }
}

std::error_code CMessageParser::parseDeleteMessage(const std::vector<uint8_t>& message) {
    if (message.size() < 16) {
        errorMessage_ = "Message too short for delete parsing";
        return std::make_error_code(std::errc::message_size);
    }

    try {
        auto headerResult = parseMessageHeader(message);
        if (headerResult) return headerResult;

        auto bodyResult = parseMessageBody(message);
        if (bodyResult) return bodyResult;

        parsedDocument_ = parsedQuery_;

        return std::error_code();
    } catch (const std::exception& e) {
        errorMessage_ = "Exception during delete parsing: " + std::string(e.what());
        return std::make_error_code(std::errc::invalid_argument);
    }
}

std::error_code CMessageParser::parseMessageHeader(const std::vector<uint8_t>& message) {
    size_t offset = 0;

    uint32_t messageLength = readLittleEndian32(message, offset);
    offset += 4;

    parsedRequestId_ = readLittleEndian32(message, offset);
    offset += 4;

    parsedResponseTo_ = readLittleEndian32(message, offset);
    offset += 4;

    uint32_t opCode = readLittleEndian32(message, offset);
    offset += 4;

    parsedHeader_.messageLength = messageLength;
    parsedHeader_.requestID = parsedRequestId_;
    parsedHeader_.responseTo = parsedResponseTo_;
    parsedHeader_.opCode = opCode;

    return std::error_code();
}

std::error_code CMessageParser::parseMessageBody(const std::vector<uint8_t>& message) {
    size_t offset = 16;

    parsedDatabase_ = parseCString(message, offset);
    if (parsedDatabase_.empty()) {
        errorMessage_ = "Failed to parse database name";
        return std::make_error_code(std::errc::invalid_argument);
    }

    parsedCollection_ = parseCString(message, offset);
    if (parsedCollection_.empty()) {
        errorMessage_ = "Failed to parse collection name";
        return std::make_error_code(std::errc::invalid_argument);
    }

    std::vector<uint8_t> queryDoc = parseDocument(message, offset);
    if (queryDoc.empty()) {
        errorMessage_ = "Failed to parse query document";
        return std::make_error_code(std::errc::invalid_argument);
    }

    parsedQuery_ = std::string(queryDoc.begin(), queryDoc.end());

    return std::error_code();
}

std::string CMessageParser::parseCString(const std::vector<uint8_t>& message, size_t& offset) {
    std::string result;

    while (offset < message.size() && message[offset] != 0) {
        result += static_cast<char>(message[offset]);
        offset++;
    }

    if (offset < message.size()) {
        offset++;
    }

    return result;
}

std::vector<uint8_t> CMessageParser::parseDocument(const std::vector<uint8_t>& message, size_t& offset) {
    if (offset + 4 > message.size()) {
        return std::vector<uint8_t>();
    }

    uint32_t docSize = readLittleEndian32(message, offset);
    if (docSize <= 0 || offset + docSize > message.size()) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> document(message.begin() + offset, message.begin() + offset + docSize);
    offset += docSize;

    return document;
}

uint32_t CMessageParser::readLittleEndian32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) {
        return 0;
    }

    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

uint64_t CMessageParser::readLittleEndian64(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 8 > data.size()) {
        return 0;
    }

    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }

    return result;
}

bool CMessageParser::isValidBSONDocument(const std::vector<uint8_t>& document) {
    if (document.size() < 5) {
        return false;
    }

    uint32_t docSize = readLittleEndian32(document, 0);
    return docSize == document.size();
}

} /* namespace FauxDB */
