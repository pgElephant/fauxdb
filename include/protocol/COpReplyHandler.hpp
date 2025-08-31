#pragma once
#include "CBsonType.hpp"
#include "CDocumentWireProtocol.hpp"

#include <string>
#include <vector>
using namespace std;
namespace FauxDB
{

using namespace FauxDB;

struct OpReplyResponse
{
	int32_t responseFlags;
	int64_t cursorID;
	int32_t startingFrom;
	int32_t numberReturned;
	std::vector<std::vector<uint8_t>> documents;
};

class COpReplyHandler
{
  public:
	COpReplyHandler();


	std::vector<uint8_t> handleLegacyQuery(const std::vector<uint8_t>& message);
	std::vector<uint8_t>
	handleIsMasterQuery(const std::vector<uint8_t>& message);

	std::vector<uint8_t> buildReply(const OpReplyResponse& response,
									int32_t requestID);
	std::vector<uint8_t> buildIsMasterReply(int32_t requestID);

  private:
	bool parseLegacyQuery(const std::vector<uint8_t>& message,
						  std::string& collection, std::vector<uint8_t>& query);
	std::vector<uint8_t> serializeReply(const OpReplyResponse& response,
										int32_t requestID);
	std::vector<uint8_t> serializeBsonDocument(const std::vector<uint8_t>& doc);
};

} /* namespace FauxDB */
