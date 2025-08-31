
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
	config.serverName = "TestClient";
	config.maxConnections = 1;
	config.workerThreads = 1;

	CTcp client(config);
	cout << "Initializing TCP client..." << endl;
	error_code ec = client.initialize();
	if (ec.value() != 0)
	{
		cerr << "Failed to initialize client: " << ec.message() << endl;
		return 1;
	}
	cout << "Starting TCP client..." << endl;
	ec = client.start();
	if (ec.value() != 0)
	{
		cerr << "Failed to start client: " << ec.message() << endl;
		return 2;
	}
	cout << "Client running. Press Enter to stop..." << endl;
	cin.get();
	client.stop();
	cout << "Client stopped." << endl;
	return 0;
}
