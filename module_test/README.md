# FauxDB Network Module Test

This folder contains simple test programs for the FauxDB network module:

- `network_server.cpp`: Starts a server, waits for a client, exchanges messages.
- `network_client.cpp`: Connects to the server, exchanges messages.

## Usage

1. Build both programs:
   - `g++ -I../../include network_server.cpp -o network_server`
   - `g++ -I../../include network_client.cpp -o network_client`
2. Run the server in one terminal:
   - `./network_server`
3. Run the client in another terminal:
   - `./network_client`

You should see message exchange between client and server.
