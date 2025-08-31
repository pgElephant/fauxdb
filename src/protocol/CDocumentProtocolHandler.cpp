#include "CDocumentProtocolHandler.hpp"

#include "CBsonType.hpp"
#include "CLogger.hpp"

#include <algorithm>
#include <cstring>

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
	(void)bytesRead;
	(void)queryTranslator;
	(void)responseBuilder;
	if (buffer.empty() || bytesRead <= 0)
	{
		return createErrorBsonDocument(-1, "Empty buffer");
	}

	auto message = parser_->parseMessage(buffer);
	if (!message)
	{
		return createErrorBsonDocument(-2, "Failed to parse message");
	}

	if (!validateMessage(*message))
	{
		return createErrorBsonDocument(-3, "Invalid message format");
	}

	string command = extractCommandName(*message);
	auto it = commandHandlers_.find(command);
	if (it == commandHandlers_.end())
	{
		return createErrorBsonDocument(-4, "Unknown command: " + command);
	}

	auto response = it->second->handleCommand(*message);
	if (!response)
	{
		return createErrorBsonDocument(-5, "Command handler failed");
	}

	messageCount_++;

	return createHelloBsonDocument();
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
	(void)request;
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
