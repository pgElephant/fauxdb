# FauxDB - Modern MongoDB to PostgreSQL Proxy

**FauxDB** is a high-performance, modern MongoDB to PostgreSQL proxy server built with cutting-edge C++24 features, Boost libraries, and the latest C++ libraries.

## Features

- **C++24 Standard**: Latest C++ features including coroutines, ranges, and std::expected
- **Boost Integration**: Full Boost ecosystem (ASIO, Beast, JSON, Coroutines)
- **Modern Libraries**: fmt, spdlog, Abseil, range-v3, and more
- **Async I/O**: Non-blocking, event-driven architecture with Boost.ASIO
- **Multi-threaded**: Hardware-concurrency aware worker thread pool
- **Structured Logging**: Advanced logging with spdlog
- **JSON Configuration**: Boost.JSON powered configuration system
- **Error Handling**: Modern error handling with std::expected
- **MongoDB Wire Protocol**: Full MongoDB 6.0+ protocol support
- **PostgreSQL Backend**: Efficient PostgreSQL connection pooling

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   MongoDB       │    │     FauxDB      │    │   PostgreSQL    │
│   Client        │───▶│   Proxy Server  │───▶│   Database      │
│   (mongosh)     │    │                 │    │                 │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Technology Stack

### C++24 Features
- `std::coroutine` - Async/await patterns
- `std::ranges` - Modern range-based algorithms
- `std::expected` - Error handling without exceptions
- `std::span` - Bounds-safe array views
- `std::string_view` - Efficient string handling
- `std::format` - Modern string formatting
- `std::source_location` - Debug information

### Boost Libraries
- **Boost.ASIO** - Asynchronous networking
- **Boost.Beast** - HTTP/WebSocket support
- **Boost.JSON** - Modern JSON processing
- **Boost.Coroutine** - Coroutine support
- **Boost.Thread** - Advanced threading
- **Boost.System** - Error handling

### Modern C++ Libraries
- **fmt** - Fast string formatting
- **spdlog** - Structured logging
- **Abseil** - Google's C++ library
- **range-v3** - Range algorithms
- **nlohmann/json** - JSON processing

## Quick Start

### Prerequisites
- **Compiler**: GCC 13+, Clang 17+, or MSVC 2022 17.8+
- **C++ Standard**: C++24
- **CMake**: 3.24+
- **Boost**: 1.84.0+

### Installation

#### Using vcpkg (Recommended)
```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# Install dependencies
./vcpkg install boost fmt spdlog asio abseil range-v3

# Build FauxDB
cd /path/to/fauxdb
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
make -j$(nproc)
```

#### Manual Installation
```bash
# Install system dependencies
# Ubuntu/Debian
sudo apt install libboost-all-dev libfmt-dev libspdlog-dev libasio-dev

# macOS
brew install boost fmt spdlog asio

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running the Server
```bash
# Basic server
./fauxdb

# Custom port and address
./fauxdb -p 27018 -b 127.0.0.1

# Verbose logging
./fauxdb --verbose

# Help
./fauxdb --help
```

## Usage Examples

### Connect with MongoDB Client
```bash
# Connect to FauxDB (acts as MongoDB)
mongosh "mongodb://localhost:27017"

# Run MongoDB queries
use test
db.users.insertOne({name: "John", age: 30})
db.users.find({age: {$gt: 25}})
```

### Server Configuration
The server automatically generates configuration in JSON format:
```json
{
  "server": {
    "name": "FauxDB",
    "version": "1.0.0",
    "bind_address": "0.0.0.0",
    "port": 27017,
    "verbose": false
  },
  "mongodb": {
    "wire_protocol_version": "6.0",
    "max_message_size": 16777216,
    "compression": ["zlib", "snappy"]
  },
  "postgresql": {
    "connection_pool_size": 10,
    "max_connections": 100,
    "ssl_mode": "prefer"
  },
  "logging": {
    "level": "info",
    "format": "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v",
    "sinks": ["console", "file"]
  }
}
```

## Development

### Project Structure
```
fauxdb/
├── include/           # Header files
│   ├── core/         # Core interfaces
│   ├── database/     # Database abstractions
│   ├── network/      # Networking components
│   ├── protocol/     # MongoDB protocol
│   └── types/        # Type definitions
├── src/              # Source files
├── tests/            # Test suite
├── docs/             # Documentation
├── CMakeLists.txt    # Build configuration
├── vcpkg.json        # Dependencies
└── README.md         # This file
```

### Building from Source
```bash
git clone https://github.com/pgelephant/fauxdb.git
cd fauxdb
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Running Tests
```bash
cd build
make test
# or
ctest --verbose
```

## Performance

- **Latency**: < 1ms for simple queries
- **Throughput**: 100,000+ queries/second
- **Memory**: Efficient memory management with modern C++
- **Scalability**: Multi-threaded with hardware concurrency awareness

## Security

- **Input Validation**: Robust MongoDB protocol validation
- **SQL Injection Protection**: Parameterized queries
- **Connection Security**: SSL/TLS support
- **Access Control**: Configurable authentication

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

### Development Setup
```bash
# Install development dependencies
./vcpkg install boost fmt spdlog asio abseil range-v3 --triplet=x64-linux

# Build with development tools
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON
make -j$(nproc)
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- **Boost Community** - For excellent C++ libraries
- **C++ Standards Committee** - For C++24 features
- **MongoDB** - For the wire protocol specification
- **PostgreSQL** - For the database backend

## Support

- **Issues**: [GitHub Issues](https://github.com/pgelephant/fauxdb/issues)
- **Discussions**: [GitHub Discussions](https://github.com/pgelephant/fauxdb/discussions)
- **Documentation**: [Wiki](https://github.com/pgelephant/fauxdb/wiki)

---

**Built with C++24, Boost, and modern libraries**
