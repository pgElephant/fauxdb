/*-------------------------------------------------------------------------
 *
 * CDocumentProtocolHandler.hpp
 *      Document Protocol Handler for FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "CDocumentWireProtocol.hpp"
#include "query_response/CQueryTranslator.hpp"
#include "query_response/CResponseBuilder.hpp"

#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

namespace FauxDB
{

using std::unique_ptr;
using std::string;
using std::vector;
using std::unordered_map;
using std::function;

class IDocumentCommandHandler
{
public:
	virtual ~IDocumentCommandHandler() = default;

	virtual unique_ptr<CDocumentWireMessage>
	handleCommand(const CDocumentWireMessage& request) = 0;
	virtual vector<string> getSupportedCommands() const = 0;
	virtual bool isCommandSupported(const string& command) const = 0;
};

class CDocumentProtocolHandler
{
public:
	CDocumentProtocolHandler();
	~CDocumentProtocolHandler();

	bool initialize();
	void shutdown();
	bool isInitialized() const;

	unique_ptr<CDocumentWireMessage>
	processMessage(const vector<uint8_t>& messageData);

	/* Protocol stub methods */
	bool start();
	void stop();
	bool isRunning() const;
	std::vector<std::string> getSupportedCommands() const;
	vector<uint8_t>
	processDocumentMessage(const vector<uint8_t>& messageData, ssize_t bytesRead, 
						  CQueryTranslator& queryTranslator, CResponseBuilder& responseBuilder);

	void registerCommandHandler(const string& command,
							  unique_ptr<IDocumentCommandHandler> handler);
	void unregisterCommandHandler(const string& command);

private:
	bool initialized_;
	bool isRunning_;
	size_t maxBsonSize_;
	bool compressionEnabled_;
	bool checksumEnabled_;
	size_t messageCount_;
	size_t errorCount_;
	size_t compressedMessageCount_;
	unordered_map<string, unique_ptr<IDocumentCommandHandler>> commandHandlers_;
	unique_ptr<CDocumentWireParser> wireParser_;
	unique_ptr<CDocumentWireParser> parser_;
	unique_ptr<CQueryTranslator> queryTranslator_;
	unique_ptr<CResponseBuilder> responseBuilder_;

	bool parseMessage(const vector<uint8_t>& messageData,
					 CDocumentWireMessage& message);
	unique_ptr<CDocumentWireMessage>
	executeCommand(const string& command, const CDocumentWireMessage& request);
	
	void initializeConfiguration();
	void initializeDefaultCommandHandlers();
	vector<uint8_t> createErrorBsonDocument(int errorCode, const string& errorMessage);
	
	// Additional methods referenced in implementation
	unique_ptr<CDocumentWireMessage> routeCommand(const CDocumentWireMessage& request);
	unique_ptr<CDocumentWireMessage> createHelloResponse(int32_t requestID);
	unique_ptr<CDocumentWireMessage> createBuildInfoResponse(int32_t requestID);
	unique_ptr<CDocumentWireMessage> createIsMasterResponse(int32_t requestID);
	unique_ptr<CDocumentWireMessage> createErrorResponse(int32_t requestID, int32_t errorCode, const string& errorMessage);
	vector<uint8_t> createBsonDocument(const unordered_map<string, string>& fields);
	vector<uint8_t> createBsonDocument(const unordered_map<string, int32_t>& fields);
	vector<uint8_t> createBsonDocument(const unordered_map<string, double>& fields);
	vector<uint8_t> createBsonDocument(const unordered_map<string, bool>& fields);
	void setMaxBsonSize(size_t size);
	void setCompressionEnabled(bool enabled);
	void setChecksumEnabled(bool enabled);
	bool validateMessage(const CDocumentWireMessage& message) const;
	unique_ptr<CDocumentWireMessage> processMessage(const CDocumentWireMessage& message);
	string extractCommandName(const CDocumentWireMessage& message);
	vector<uint8_t> createHelloBsonDocument();
	
	// Additional methods from implementation
	void logError(const string& operation, const string& error);
	unique_ptr<CDocumentWireMessage> handleCommonCommand(const CDocumentWireMessage& request, const string& commandName);
	vector<uint8_t> createBuildInfoBsonDocument();
	vector<uint8_t> createIsMasterBsonDocument();
	vector<uint8_t> createBsonArray(const string& key, const vector<string>& values);
};

class CHelloCommandHandler : public IDocumentCommandHandler
{
public:
	CHelloCommandHandler();
	~CHelloCommandHandler();

	unique_ptr<CDocumentWireMessage>
	handleCommand(const CDocumentWireMessage& request) override;
	vector<string> getSupportedCommands() const override;
	bool isCommandSupported(const string& command) const override;

private:
	vector<uint8_t> createHelloResponseDocument();
};

class CBuildInfoCommandHandler : public IDocumentCommandHandler
{
public:
	CBuildInfoCommandHandler();
	~CBuildInfoCommandHandler();

	unique_ptr<CDocumentWireMessage>
	handleCommand(const CDocumentWireMessage& request) override;
	vector<string> getSupportedCommands() const override;
	bool isCommandSupported(const string& command) const override;

private:
	vector<uint8_t> createBuildInfoResponseDocument();
};

class CIsMasterCommandHandler : public IDocumentCommandHandler
{
public:
	CIsMasterCommandHandler();
	~CIsMasterCommandHandler();

	unique_ptr<CDocumentWireMessage>
	handleCommand(const CDocumentWireMessage& request) override;
	vector<string> getSupportedCommands() const override;
	bool isCommandSupported(const string& command) const override;

private:
	vector<uint8_t> createIsMasterResponseDocument();
};

} /* namespace FauxDB */
