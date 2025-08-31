





#include "CServer.hpp"

using namespace std;

namespace FauxDB
{


void CServerConfig::setDefaults()
{
	serverName = "FauxDB";
	bindAddress = "0.0.0.0";
	port = 27017;
	maxConnections = 1000;
	workerThreads = 4;
	startupTimeout = std::chrono::milliseconds(30000);
	shutdownTimeout = std::chrono::milliseconds(30000);
	enableSSL = false;
	sslCertPath = "";
	sslKeyPath = "";
	logLevel = "INFO";
	configFile = "fauxdb.conf";
}


bool CServerConfig::loadFromFile(const std::string& filename)
{
	(void)filename;
	return true;
}


bool CServerConfig::validate() const
{
	if (port == 0 || port > 65535)
		return false;
	if (maxConnections == 0)
		return false;
	if (workerThreads == 0)
		return false;
	return true;
}

} /* namespace FauxDB */
