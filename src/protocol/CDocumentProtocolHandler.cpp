#include "CDocumentProtocolHandler.hpp"

#include "CBsonType.hpp"
#include "CLogger.hpp"

#include <algorithm>
#include <cstring>
#include <chrono>
#include <string>
#include <vector>

using namespace std;
using namespace FauxDB;

namespace FauxDB
{

CDocumentProtocolHandler::CDocumentProtocolHandler()
	: initialized_(false), isRunning_(false), maxBsonSize_(16777216),
	  compressionEnabled_(false), checksumEnabled_(false), messageCount_(0),
	  errorCount_(0), compressedMessageCount_(0)
{
	initializeConfiguration();
}

CDocumentProtocolHandler::~CDocumentProtocolHandler()
{
	shutdown();
}

bool CDocumentProtocolHandler::initialize()
{
	if (initialized_)
		return true;
	parser_ = make_unique<CDocumentWireParser>();
	initializeDefaultCommandHandlers();
	initialized_ = true;
	return true;
}

void CDocumentProtocolHandler::shutdown()
{
	if (!initialized_)
		return;
	commandHandlers_.clear();
	parser_.reset();
	initialized_ = false;
}

bool CDocumentProtocolHandler::start()
{
	if (!initialized_)
		return false;
	isRunning_ = true;
	return true;
}

void CDocumentProtocolHandler::stop()
{
	if (!isRunning_)
		return;
	isRunning_ = false;
}

bool CDocumentProtocolHandler::isRunning() const
{
	return isRunning_;
}

void CDocumentProtocolHandler::registerCommandHandler(
	const string& command, unique_ptr<IDocumentCommandHandler> handler)
{
	if (handler)
		commandHandlers_[command] = std::move(handler);
}

void CDocumentProtocolHandler::unregisterCommandHandler(const string& command)
{
	commandHandlers_.erase(command);
}

vector<uint8_t> CDocumentProtocolHandler::processDocumentMessage(
    const vector<uint8_t>& buffer, ssize_t bytesRead,
    CQueryTranslator& queryTranslator,
    CResponseBuilder& responseBuilder)
{
    (void)queryTranslator;
    (void)responseBuilder;
    if (buffer.empty() || bytesRead <= 0)
    {
        return createErrorBsonDocument(-1, "Empty buffer");
    }

    // Parse full wire message
    auto message = parser_->parseMessage(buffer);
    if (!message || !message->isValid())
    {
        return createErrorBsonDocument(-2, "Invalid wire message");
    }

    // Validate message length upper bound (48MB)
    if (message->getTotalSize() > 48000000)
    {
        return createErrorBsonDocument(-3, "Message too large");
    }

    const auto& hdr = message->getHeader();
    int32_t requestId = hdr.requestID;

    // Extract command and $db from Kind-0 document when OP_MSG
    std::string commandName = "";
    std::string database = "admin";
    if (message->isOpMsg() && message->getMsgBody() &&
        !message->getMsgBody()->sections0.empty() &&
        message->getMsgBody()->sections0.front())
    {
        const auto& doc = message->getMsgBody()->sections0.front()->bsonDoc;
        // BSON: int32 size, then elements; first element key is command name
        if (doc.size() >= 5)
        {
            size_t off = 4; // skip size
            if (off < doc.size())
            {
                uint8_t type = doc[off++];
                (void)type; // first element type is ignored for command name extraction
                // read cstring key
                size_t start = off;
                while (off < doc.size() && doc[off] != 0x00) off++;
                if (off < doc.size())
                {
                    commandName.assign(reinterpret_cast<const char*>(&doc[start]), off - start);
                    off++; // skip null
                }

                // Scan for $db string key
                size_t cur = off;
                while (cur < doc.size())
                {
                    uint8_t etype = doc[cur++];
                    if (etype == 0x00) break; // end of document
                    // field name
                    size_t kstart = cur;
                    while (cur < doc.size() && doc[cur] != 0x00) cur++;
                    if (cur >= doc.size()) break;
                    std::string key(reinterpret_cast<const char*>(&doc[kstart]), cur - kstart);
                    cur++; // skip null
                    // handle value skip
                    auto skipOrRead = [&](uint8_t t) -> bool {
                        if (t == 0x01 || t == 0x09) { // double or datetime
                            if (cur + 8 > doc.size()) return false; cur += 8; return true;
                        }
                        if (t == 0x02) { // string
                            if (cur + 4 > doc.size()) return false; int32_t sl=0; std::memcpy(&sl,&doc[cur],4); cur += 4; if (sl < 0 || cur + static_cast<size_t>(sl) > doc.size()) return false; if (key == "$db") { database.assign(reinterpret_cast<const char*>(&doc[cur]), sl ? sl - 1 : 0); } cur += sl; return true;
                        }
                        if (t == 0x03 || t == 0x04) { // doc/array
                            if (cur + 4 > doc.size()) return false; int32_t dl=0; std::memcpy(&dl,&doc[cur],4); if (dl <= 0 || cur + static_cast<size_t>(dl) > doc.size()) return false; cur += dl; return true;
                        }
                        if (t == 0x05) { // binary
                            if (cur + 4 > doc.size()) return false; int32_t bl=0; std::memcpy(&bl,&doc[cur],4); cur += 4; if (cur + 1 + static_cast<size_t>(bl) > doc.size()) return false; cur += 1 + bl; return true;
                        }
                        if (t == 0x06 || t == 0x0A) { // undefined / null
                            return true;
                        }
                        if (t == 0x07) { // objectId
                            if (cur + 12 > doc.size()) return false; cur += 12; return true;
                        }
                        if (t == 0x08) { // bool
                            if (cur + 1 > doc.size()) return false; cur += 1; return true;
                        }
                        if (t == 0x10) { // int32
                            if (cur + 4 > doc.size()) return false; cur += 4; return true;
                        }
                        if (t == 0x12) { // int64
                            if (cur + 8 > doc.size()) return false; cur += 8; return true;
                        }
                        // Unknown type: stop
                        return false;
                    };
                    if (!skipOrRead(etype)) break;
                }
            }
        }
    }

    // Default to hello if unrecognized
    if (commandName.empty()) commandName = "hello";

    // Build response document based on commandName
    CBsonType bson;
    if (!bson.initialize() || !bson.beginDocument())
    {
        return createErrorBsonDocument(-6, "BSON init failed");
    }

    if (commandName == "hello" || commandName == "isMaster")
    {
        bson.addDouble("ok", 1.0);
        bson.addBool("isWritablePrimary", true);
        bson.addInt32("maxBsonObjectSize", 16777216);
        bson.addInt32("maxMessageSizeBytes", 48000000);
        bson.addInt32("maxWriteBatchSize", 100000);
        auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now());
        bson.addDateTime("localTime", now.time_since_epoch().count());
        bson.addInt32("minWireVersion", 6);
        bson.addInt32("maxWireVersion", 6);
    }
    else if (commandName == "ping")
    {
        bson.addDouble("ok", 1.0);
    }
    else if (commandName == "buildInfo")
    {
        bson.addDouble("ok", 1.0);
        bson.addString("version", "1.0.0");
        bson.addString("gitVersion", "fauxdb-1.0.0");
        bson.addString("modules", "none");
        bson.addString("allocator", "system");
        bson.addString("javascriptEngine", "none");
        bson.addString("sysInfo", "FauxDB Server");
    }
    else
    {
        // Unknown command
        bson.clear();
        bson.initialize();
        bson.beginDocument();
        bson.addDouble("ok", 0.0);
        bson.addInt32("code", 59);
        bson.addString("codeName", "CommandNotFound");
        bson.addString("errmsg", "no such command: " + commandName);
    }

    if (!bson.endDocument())
    {
        return createErrorBsonDocument(-7, "BSON finalize failed");
    }

    // Wrap into OP_MSG reply
    CDocumentMsgBody body;
    body.flagBits = 0;
    auto sec0 = std::make_unique<CDocumentMsgSection0>();
    sec0->bsonDoc = bson.getDocument();
    body.addSection0(std::move(sec0));

    CDocumentMessageHeader header;
    header.requestID = static_cast<int32_t>(++messageCount_);
    header.responseTo = requestId;
    header.opCode = static_cast<int32_t>(CDocumentOpCode::OP_MSG);

    CDocumentWireMessage reply;
    reply.setHeader(header);
    reply.setMsgBody(body);
    return reply.serializeToBytes();
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::routeCommand(const CDocumentWireMessage& request)
{
	string cmd = extractCommandName(request);
	auto it = commandHandlers_.find(cmd);
	if (it != commandHandlers_.end())
	{
		return it->second->handleCommand(request);
	}
	return nullptr;
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::createHelloResponse(int32_t requestID)
{
	return CDocumentWireMessage::createHelloResponse(requestID);
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::createBuildInfoResponse(int32_t requestID)
{
	return CDocumentWireMessage::createBuildInfoResponse(requestID);
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::createIsMasterResponse(int32_t requestID)
{
	return CDocumentWireMessage::createIsMasterResponse(requestID);
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::createErrorResponse(int32_t requestID, int32_t errorCode,
										   const string& errorMessage)
{
	(void)requestID;

	auto errorDoc = createErrorBsonDocument(errorCode, errorMessage);

	CDocumentReplyBody replyBody;
	replyBody.document = errorDoc;
	auto msg = make_unique<CDocumentWireMessage>();
	msg->setReplyBody(replyBody);
	return msg;
}

vector<uint8_t> CDocumentProtocolHandler::createBsonDocument(
	const unordered_map<string, string>& fields)
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	for (const auto& [key, value] : fields)
	{
		bson.addString(key, value);
	}
	bson.endDocument();
	return bson.getDocument();
}

vector<uint8_t> CDocumentProtocolHandler::createBsonDocument(
	const unordered_map<string, int32_t>& fields)
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	for (const auto& [key, value] : fields)
	{
		bson.addInt32(key, value);
	}
	bson.endDocument();
	return bson.getDocument();
}

vector<uint8_t> CDocumentProtocolHandler::createBsonDocument(
	const unordered_map<string, double>& fields)
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	for (const auto& [key, value] : fields)
	{
		bson.addDouble(key, value);
	}
	bson.endDocument();
	return bson.getDocument();
}

vector<uint8_t> CDocumentProtocolHandler::createBsonDocument(
	const unordered_map<string, bool>& fields)
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	for (const auto& [key, value] : fields)
	{
		bson.addBool(key, value);
	}
	bson.endDocument();
	return bson.getDocument();
}

void CDocumentProtocolHandler::setMaxBsonSize(size_t size)
{
	maxBsonSize_ = size;
}

void CDocumentProtocolHandler::setCompressionEnabled(bool enabled)
{
	compressionEnabled_ = enabled;
}

void CDocumentProtocolHandler::setChecksumEnabled(bool enabled)
{
	checksumEnabled_ = enabled;
}

vector<string> CDocumentProtocolHandler::getSupportedCommands() const
{
	vector<string> commands;
	for (const auto& [cmd, _] : commandHandlers_)
	{
		commands.push_back(cmd);
	}
	return commands;
}

bool CDocumentProtocolHandler::validateMessage(
	const CDocumentWireMessage& message) const
{
	if (message.getTotalSize() > maxBsonSize_)
	{
		return false;
	}

	return true;
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::processMessage(const CDocumentWireMessage& message)
{
	string command = extractCommandName(message);
	auto it = commandHandlers_.find(command);
	if (it != commandHandlers_.end())
	{
		return it->second->handleCommand(message);
	}

	return createErrorResponse(0, -4, "Unknown command: " + command);
}

void CDocumentProtocolHandler::logError(const string& operation,
									 const string& details)
{
	errorCount_++;

	(void)operation;
	(void)details;
}

void CDocumentProtocolHandler::initializeDefaultCommandHandlers()
{
	commandHandlers_["hello"] = make_unique<CHelloCommandHandler>();
	commandHandlers_["buildInfo"] = make_unique<CBuildInfoCommandHandler>();
	commandHandlers_["isMaster"] = make_unique<CIsMasterCommandHandler>();
}

void CDocumentProtocolHandler::initializeConfiguration()
{
	maxBsonSize_ = 16777216;
	compressionEnabled_ = false;
	checksumEnabled_ = false;
}

string
CDocumentProtocolHandler::extractCommandName(const CDocumentWireMessage& request)
{
	// Try to extract command name from first key of Kind-0 BSON
	if (request.isOpMsg() && request.getMsgBody() &&
		!request.getMsgBody()->sections0.empty() &&
		request.getMsgBody()->sections0.front())
	{
		const auto& doc = request.getMsgBody()->sections0.front()->bsonDoc;
		if (doc.size() >= 5)
		{
			size_t off = 4; // skip size
			if (off < doc.size())
			{
				// Skip type byte, read key cstring
				off++;
				size_t start = off;
				while (off < doc.size() && doc[off] != 0x00) off++;
				if (off < doc.size())
				{
					return std::string(reinterpret_cast<const char*>(&doc[start]), off - start);
				}
			}
		}
	}
	return "hello";
}

unique_ptr<CDocumentWireMessage>
CDocumentProtocolHandler::handleCommonCommand(const CDocumentWireMessage& request,
										   const string& commandName)
{
	auto it = commandHandlers_.find(commandName);
	if (it != commandHandlers_.end())
	{
		return it->second->handleCommand(request);
	}

	return createErrorResponse(0, -4, "Unknown command: " + commandName);
}

vector<uint8_t> CDocumentProtocolHandler::createHelloBsonDocument()
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	bson.addString("isWritablePrimary", "true");
	bson.addString("msg", "hello");
	bson.addInt32("maxBsonObjectSize", maxBsonSize_);
	bson.addInt32("maxMessageSizeBytes", 48000000);
	bson.addInt32("maxWriteBatchSize", 100000);
	bson.addString("hello", "world");
	bson.endDocument();
	return bson.getDocument();
}

vector<uint8_t> CDocumentProtocolHandler::createBuildInfoBsonDocument()
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	bson.addString("version", "4.4.0");
	bson.addString("gitVersion", "abcdef1234567890");
	bson.addInt32("modules", 0);
	bson.addString("allocator", "system");
	bson.addString("javascriptEngine", "mozjs");
	bson.addString("sysInfo", "mocked");
	bson.endDocument();
	return bson.getDocument();
}

vector<uint8_t> CDocumentProtocolHandler::createIsMasterBsonDocument()
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	bson.addString("ismaster", "true");
	bson.addString("msg", "isMaster");
	bson.addInt32("maxBsonObjectSize", maxBsonSize_);
	bson.addInt32("maxMessageSizeBytes", 48000000);
	bson.addInt32("maxWriteBatchSize", 100000);
	bson.endDocument();
	return bson.getDocument();
}

vector<uint8_t>
CDocumentProtocolHandler::createErrorBsonDocument(int32_t errorCode,
											   const string& errorMessage)
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	bson.addInt32("code", errorCode);
	bson.addString("errmsg", errorMessage);
	bson.endDocument();
	return bson.getDocument();
}

vector<uint8_t>
CDocumentProtocolHandler::createBsonArray(const string& key,
									   const vector<string>& values)
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	bson.beginArray(key);
	for (const auto& value : values)
	{
		bson.addString("", value);
	}
	bson.endArray();
	bson.endDocument();
	return bson.getDocument();
}

} // namespace FauxDB

// Command Handler Implementations
namespace FauxDB
{

CHelloCommandHandler::CHelloCommandHandler()
{
}

CHelloCommandHandler::~CHelloCommandHandler()
{
}

std::unique_ptr<CDocumentWireMessage>
CHelloCommandHandler::handleCommand(const CDocumentWireMessage& request)
{
	(void)request;
	auto response = std::make_unique<CDocumentWireMessage>();
	// TODO: Implement hello response
	return response;
}

std::vector<std::string> CHelloCommandHandler::getSupportedCommands() const
{
	return {"hello"};
}

bool CHelloCommandHandler::isCommandSupported(const std::string& command) const
{
	return command == "hello";
}

std::vector<uint8_t> CHelloCommandHandler::createHelloResponseDocument()
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	bson.addString("ok", "1.0");
	bson.addString("msg", "hello");
	bson.endDocument();
	return bson.getDocument();
}

CBuildInfoCommandHandler::CBuildInfoCommandHandler()
{
}

CBuildInfoCommandHandler::~CBuildInfoCommandHandler()
{
}

std::unique_ptr<CDocumentWireMessage>
CBuildInfoCommandHandler::handleCommand(const CDocumentWireMessage& request)
{
	(void)request;
	auto response = std::make_unique<CDocumentWireMessage>();
	// TODO: Implement buildInfo response
	return response;
}

std::vector<std::string> CBuildInfoCommandHandler::getSupportedCommands() const
{
	return {"buildInfo"};
}

bool CBuildInfoCommandHandler::isCommandSupported(const std::string& command) const
{
	return command == "buildInfo";
}

std::vector<uint8_t> CBuildInfoCommandHandler::createBuildInfoResponseDocument()
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	bson.addString("version", "4.4.0");
	bson.addString("gitVersion", "abcdef1234567890");
	bson.endDocument();
	return bson.getDocument();
}

CIsMasterCommandHandler::CIsMasterCommandHandler()
{
}

CIsMasterCommandHandler::~CIsMasterCommandHandler()
{
}

std::unique_ptr<CDocumentWireMessage>
CIsMasterCommandHandler::handleCommand(const CDocumentWireMessage& request)
{
	(void)request;
	auto response = std::make_unique<CDocumentWireMessage>();
	// TODO: Implement isMaster response
	return response;
}

std::vector<std::string> CIsMasterCommandHandler::getSupportedCommands() const
{
	return {"isMaster"};
}

bool CIsMasterCommandHandler::isCommandSupported(const std::string& command) const
{
	return command == "isMaster";
}

std::vector<uint8_t> CIsMasterCommandHandler::createIsMasterResponseDocument()
{
	CBsonType bson;
	bson.initialize();
	bson.beginDocument();
	bson.addBool("ismaster", true);
	bson.addString("msg", "isMaster");
	bson.endDocument();
	return bson.getDocument();
}

} // namespace FauxDB
