
#include "../include/CServerConfig.hpp"
#include "../include/network/CTcp.hpp"

#include <iostream>
using namespace std;
using namespace FauxDB::Network;
using namespace FauxDB::Server;

int main()
{
	CServerConfig config;
	config.port = 5555;
	config.bindAddress = "127.0.0.1";
	config.serverName = "TestServer";
	config.maxConnections = 10;
	config.workerThreads = 2;

	CTcp server(config);
	cout << "Initializing TCP server..." << endl;
	error_code ec = server.initialize();
	if (ec.value() != 0)
	{
		cerr << "Failed to initialize server: " << ec.message() << endl;
		return 1;
	}
	cout << "Starting TCP server..." << endl;
	ec = server.start();
	if (ec.value() != 0)
	{
		cerr << "Failed to start server: " << ec.message() << endl;
		return 2;
	}
	cout << "Server running. Press Enter to stop..." << endl;
	cin.get();
	server.stop();
	cout << "Server stopped." << endl;
	return 0;
}
