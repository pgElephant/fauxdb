# Development Guide

## Getting Started

### Development Environment Setup

#### Prerequisites

1. **System Dependencies**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install build-essential cmake git gdb valgrind
   sudo apt-get install libpq-dev libmongoc-1.0-dev nlohmann-json3-dev
   
   # macOS
   brew install cmake git lldb
   brew install postgresql mongo-c-driver nlohmann-json
   
   # Development tools
   brew install clang-format clang-tidy cppcheck
   ```

2. **IDE/Editor Setup**
   - **VS Code**: Install C++ extension pack
   - **CLion**: Configure CMake and debugger
   - **Vim/Neovim**: Install language servers and plugins

#### Repository Setup

```bash
# Clone repository
git clone https://github.com/yourusername/fauxdb.git
cd fauxdb

# Create development branch
git checkout -b feature/your-feature-name

# Setup development environment
mkdir build-dev && cd build-dev
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ..
make -j$(nproc)
```

#### Development Configuration

```bash
# Set development environment variables
export FAUXDB_DEV=1
export FAUXDB_LOG_LEVEL=7
export FAUXDB_DEBUG=true

# Create development config
cp ../config/fauxdb.conf fauxdb-dev.conf
# Edit fauxdb-dev.conf for development settings
```

## Project Structure

### Directory Layout

```
fauxdb/
├── CMakeLists.txt              # Main build configuration
├── README.md                   # Project overview
├── LICENSE                     # License file
├── .gitignore                  # Git ignore rules
├── .clang-format              # Code formatting rules
├── .github/                   # GitHub workflows
│   └── workflows/
│       ├── ci.yml            # Continuous integration
│       └── release.yml       # Release automation
├── build/                     # Build output (ignored)
├── config/                    # Configuration files
│   ├── fauxdb.conf           # Sample configuration
│   ├── fauxdb.service        # SystemD service file
│   └── docker/               # Docker configurations
├── doc/                       # Documentation
│   ├── installation.md
│   ├── configuration.md
│   ├── usage.md
│   ├── architecture.md
│   └── development.md
├── include/                   # Header files
│   ├── config.hpp            # Configuration management
│   ├── logger.hpp            # Logging system
│   ├── types.hpp             # Type definitions
│   ├── database_interface.hpp # Database abstraction
│   ├── protocol_handler.hpp   # MongoDB protocol
│   ├── query_translator.hpp   # Query translation
│   ├── network_manager.hpp    # Network handling
│   └── server_orchestrator.hpp # Server coordination
├── src/                       # Source files
│   ├── main.cpp              # Application entry point
│   ├── config.cpp            # Configuration implementation
│   ├── logger.cpp            # Logging implementation
│   ├── types.cpp             # Type implementations
│   ├── protocol_handler.cpp   # Protocol implementation
│   ├── query_translator.cpp   # Translation logic
│   ├── network_manager.cpp    # Network implementation
│   ├── pg_database.cpp       # PostgreSQL implementation
│   └── server_orchestrator.cpp # Server coordination
├── tests/                     # Test suite
│   ├── unit/                 # Unit tests
│   ├── integration/          # Integration tests
│   ├── performance/          # Performance tests
│   └── regression/           # Regression tests
├── tools/                     # Development tools
│   ├── scripts/              # Build and utility scripts
│   ├── docker/               # Docker development setup
│   └── benchmarks/           # Performance benchmarks
└── examples/                  # Usage examples
    ├── clients/              # Client examples
    └── configurations/       # Configuration examples
```

### Code Organization

#### Header Files (`include/`)

**Core Components**:
- `types.hpp` - Common type definitions and enums
- `config.hpp` - Configuration management interfaces
- `logger.hpp` - Logging system interfaces

**Network Layer**:
- `network_manager.hpp` - TCP connection handling
- `protocol_handler.hpp` - MongoDB wire protocol

**Data Layer**:
- `database_interface.hpp` - Abstract database interface
- `pg_database.hpp` - PostgreSQL-specific implementation
- `query_translator.hpp` - Query translation engine

**Management**:
- `server_orchestrator.hpp` - Server lifecycle management

#### Source Files (`src/`)

Each header file has a corresponding implementation file with the same name but `.cpp` extension.

## Coding Standards

### C++ Style Guide

#### Naming Conventions

```cpp
// Classes: PascalCase
class QueryTranslator {
    // Public methods: camelCase
    std::string translateQuery(const QueryContext& context);
    
    // Private members: snake_case with trailing underscore
    DatabasePool& database_pool_;
    std::unique_ptr<Logger> logger_;
};

// Functions: camelCase
bool validateConfiguration(const Config& config);

// Variables: snake_case
int connection_count = 0;
std::string error_message;

// Constants: UPPER_SNAKE_CASE
const int MAX_CONNECTIONS = 1000;
const std::string DEFAULT_CONFIG_FILE = "/etc/fauxdb/fauxdb.conf";

// Enums: PascalCase with PascalCase values
enum class ErrorCode {
    Success,
    NetworkError,
    DatabaseError
};
```

#### Code Formatting

Use `.clang-format` configuration:

```yaml
# .clang-format
BasedOnStyle: Google
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
```

Apply formatting:
```bash
# Format single file
clang-format -i src/query_translator.cpp

# Format all files
find src include -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

#### Memory Management

```cpp
// Use RAII and smart pointers
class DatabaseConnection {
    std::unique_ptr<PGconn, decltype(&PQfinish)> connection_;
    
public:
    DatabaseConnection(const std::string& connection_string)
        : connection_(PQconnectdb(connection_string.c_str()), PQfinish) {
        if (!connection_ || PQstatus(connection_.get()) != CONNECTION_OK) {
            throw DatabaseException("Failed to connect");
        }
    }
    
    // No manual memory management needed
    ~DatabaseConnection() = default;
    
    // Move-only semantics
    DatabaseConnection(const DatabaseConnection&) = delete;
    DatabaseConnection& operator=(const DatabaseConnection&) = delete;
    DatabaseConnection(DatabaseConnection&&) = default;
    DatabaseConnection& operator=(DatabaseConnection&&) = default;
};
```

#### Error Handling

```cpp
// Use exceptions for error propagation
class FauxDBException : public std::exception {
    std::string message_;
    ErrorCode code_;
    
public:
    FauxDBException(ErrorCode code, const std::string& message)
        : code_(code), message_(message) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
    
    ErrorCode code() const noexcept { return code_; }
};

// Function that can fail
DatabaseResult executeQuery(const std::string& sql) {
    if (sql.empty()) {
        throw FauxDBException(ErrorCode::InvalidQuery, "Empty SQL query");
    }
    
    // ... execution logic
}
```

### Documentation Standards

#### Header Documentation

```cpp
/**
 * @file query_translator.hpp
 * @brief MongoDB to PostgreSQL query translation engine
 * 
 * This file contains the QueryTranslator class which is responsible for
 * converting MongoDB queries into equivalent PostgreSQL SQL statements.
 * 
 * @author FauxDB Development Team
 * @date 2024-08-27
 * @version 1.0
 */

/**
 * @class QueryTranslator
 * @brief Translates MongoDB queries to PostgreSQL SQL
 * 
 * The QueryTranslator class provides methods to convert various MongoDB
 * operations (find, insert, update, delete, aggregate) into equivalent
 * PostgreSQL SQL statements that operate on JSONB data.
 * 
 * @example
 * ```cpp
 * QueryTranslator translator;
 * std::string sql = translator.translateFind(
 *     "users", 
 *     R"({"age": {"$gte": 25}})", 
 *     R"({"name": 1, "email": 1})"
 * );
 * ```
 */
class QueryTranslator {
public:
    /**
     * @brief Translate MongoDB find query to SQL
     * 
     * Converts a MongoDB find operation to an equivalent PostgreSQL
     * SELECT statement with appropriate WHERE clauses and projections.
     * 
     * @param collection Name of the collection (table)
     * @param filter MongoDB filter document as JSON string
     * @param projection MongoDB projection document as JSON string
     * @param sort Sort specification (optional)
     * @param limit Maximum number of results (optional)
     * @param skip Number of results to skip (optional)
     * 
     * @return PostgreSQL SQL SELECT statement
     * 
     * @throws TranslationException if query cannot be translated
     * 
     * @example
     * ```cpp
     * std::string sql = translator.translateFind(
     *     "users",
     *     R"({"status": "active", "age": {"$gte": 18}})",
     *     R"({"name": 1, "email": 1})",
     *     R"({"age": 1})",
     *     100,
     *     0
     * );
     * // Result: "SELECT document->'name', document->'email' FROM users 
     * //          WHERE document->>'status' = 'active' 
     * //          AND (document->>'age')::int >= 18 
     * //          ORDER BY (document->>'age')::int ASC LIMIT 100"
     * ```
     */
    std::string translateFind(
        const std::string& collection,
        const std::string& filter,
        const std::string& projection = "",
        const std::string& sort = "",
        int limit = 0,
        int skip = 0
    );
    
private:
    /**
     * @brief Convert MongoDB filter to SQL WHERE clause
     * @param filter MongoDB filter document
     * @return SQL WHERE clause
     */
    std::string buildWhereClause(const nlohmann::json& filter);
};
```

## Building and Testing

### Build System (CMake)

#### Main CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(FauxDB VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

# Compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wextra")
    if(ENABLE_SANITIZERS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
    endif()
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
endif()

# Find dependencies
find_package(PkgConfig REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(nlohmann_json REQUIRED)

pkg_check_modules(MONGOC REQUIRED libmongoc-1.0)

# Include directories
include_directories(include)
include_directories(${PostgreSQL_INCLUDE_DIRS})
include_directories(${MONGOC_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/logger.cpp
    src/types.cpp
    src/protocol_handler.cpp
    src/query_translator.cpp
    src/network_manager.cpp
    src/pg_database.cpp
    src/server_orchestrator.cpp
)

# Create executable
add_executable(fauxdb ${SOURCES})

# Link libraries
target_link_libraries(fauxdb
    ${PostgreSQL_LIBRARIES}
    ${MONGOC_LIBRARIES}
    nlohmann_json::nlohmann_json
    pthread
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install targets
install(TARGETS fauxdb DESTINATION bin)
install(FILES config/fauxdb.conf DESTINATION etc/fauxdb)
install(FILES config/fauxdb.service DESTINATION lib/systemd/system)
```

#### Building Different Configurations

```bash
# Debug build with tests
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ..
make -j$(nproc)

# Release build
mkdir build-release && cd build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Build with sanitizers
mkdir build-sanitizer && cd build-sanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
make -j$(nproc)
```

### Testing Framework

#### Unit Tests

```cpp
// tests/unit/test_query_translator.cpp
#include <gtest/gtest.h>
#include "query_translator.hpp"

class QueryTranslatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        translator = std::make_unique<QueryTranslator>();
    }
    
    std::unique_ptr<QueryTranslator> translator;
};

TEST_F(QueryTranslatorTest, TranslateSimpleFind) {
    std::string sql = translator->translateFind(
        "users",
        R"({"name": "John"})",
        ""
    );
    
    EXPECT_EQ(sql, "SELECT document FROM users WHERE document->>'name' = 'John'");
}

TEST_F(QueryTranslatorTest, TranslateComplexFind) {
    std::string sql = translator->translateFind(
        "users",
        R"({"age": {"$gte": 25}, "status": "active"})",
        R"({"name": 1, "email": 1})"
    );
    
    EXPECT_THAT(sql, ::testing::HasSubstr("(document->>'age')::int >= 25"));
    EXPECT_THAT(sql, ::testing::HasSubstr("document->>'status' = 'active'"));
    EXPECT_THAT(sql, ::testing::HasSubstr("document->'name'"));
    EXPECT_THAT(sql, ::testing::HasSubstr("document->'email'"));
}

TEST_F(QueryTranslatorTest, InvalidQueryThrowsException) {
    EXPECT_THROW(
        translator->translateFind("users", "invalid json", ""),
        TranslationException
    );
}
```

#### Integration Tests

```cpp
// tests/integration/test_protocol_integration.cpp
#include <gtest/gtest.h>
#include "protocol_handler.hpp"
#include "mock_database.hpp"

class ProtocolIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_db = std::make_shared<MockDatabase>();
        handler = std::make_unique<ProtocolHandler>(mock_db);
    }
    
    std::shared_ptr<MockDatabase> mock_db;
    std::unique_ptr<ProtocolHandler> handler;
};

TEST_F(ProtocolIntegrationTest, HandleFindCommand) {
    // Create MongoDB find command message
    std::vector<uint8_t> message = createFindMessage("users", R"({"name": "John"})");
    
    // Set up mock expectations
    EXPECT_CALL(*mock_db, executeQuery(::testing::_))
        .WillOnce(::testing::Return(createMockResult(R"([{"name": "John", "age": 30}])")));
    
    // Process message
    auto response = handler->handleMessage(message);
    
    // Verify response
    ASSERT_FALSE(response.empty());
    EXPECT_EQ(extractOpCode(response), OP_REPLY);
    
    auto documents = extractDocuments(response);
    ASSERT_EQ(documents.size(), 1);
    EXPECT_EQ(documents[0]["name"], "John");
}
```

#### Running Tests

```bash
# Run all tests
make test

# Run specific test suite
./tests/unit/test_query_translator

# Run with verbose output
ctest --verbose

# Run with coverage
make coverage
genhtml coverage.info -o coverage_report
```

## Debugging

### Debug Builds

```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
make -j$(nproc)
```

### GDB Debugging

```bash
# Start with GDB
gdb ./fauxdb

# GDB commands
(gdb) set args --config fauxdb-dev.conf --debug
(gdb) break QueryTranslator::translateFind
(gdb) run
(gdb) backtrace
(gdb) print variable_name
(gdb) continue
```

### Memory Debugging

```bash
# Valgrind memory check
valgrind --tool=memcheck --leak-check=full ./fauxdb --config fauxdb-dev.conf

# AddressSanitizer (built with -fsanitize=address)
ASAN_OPTIONS=abort_on_error=1:halt_on_error=1 ./fauxdb --config fauxdb-dev.conf
```

### Logging for Debugging

```cpp
// Use structured logging for debugging
LOG_DEBUG_FMT("Processing query: collection=%s, filter=%s", 
              collection.c_str(), filter.c_str());

// Conditional debugging
#ifdef FAUXDB_DEBUG
    LOG_DEBUG_FMT("Debug: variable_value=%d", variable_value);
#endif

// Performance measurements
auto start = std::chrono::high_resolution_clock::now();
// ... operation ...
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
LOG_DEBUG_FMT("Operation took %ld microseconds", duration.count());
```

## Performance Profiling

### Profiling Tools

#### CPU Profiling with perf

```bash
# Profile the application
perf record -g ./fauxdb --config fauxdb-dev.conf

# Analyze results
perf report

# Generate flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
```

#### Memory Profiling with Massif

```bash
# Profile memory usage
valgrind --tool=massif ./fauxdb --config fauxdb-dev.conf

# Visualize results
massif-visualizer massif.out.*
```

### Benchmarking

```cpp
// Performance test example
#include <benchmark/benchmark.h>
#include "query_translator.hpp"

static void BM_QueryTranslation(benchmark::State& state) {
    QueryTranslator translator;
    std::string filter = R"({"age": {"$gte": 25}, "status": "active"})";
    
    for (auto _ : state) {
        std::string sql = translator.translateFind("users", filter, "");
        benchmark::DoNotOptimize(sql);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_QueryTranslation);

BENCHMARK_MAIN();
```

## Continuous Integration

### GitHub Actions Workflow

```yaml
# .github/workflows/ci.yml
name: CI

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  test:
    runs-on: ubuntu-latest
    
    strategy:
      matrix:
        compiler: [gcc, clang]
        build_type: [Debug, Release]
    
    services:
      postgres:
        image: postgres:14
        env:
          POSTGRES_PASSWORD: postgres
          POSTGRES_DB: fauxdb_test
        options: >-
          --health-cmd pg_isready
          --health-interval 10s
          --health-timeout 5s
          --health-retries 5
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install Dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake build-essential libpq-dev \
                               libmongoc-1.0-dev nlohmann-json3-dev
    
    - name: Configure CMake
      run: |
        cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
                       -DCMAKE_CXX_COMPILER=${{ matrix.compiler }} \
                       -DBUILD_TESTS=ON
    
    - name: Build
      run: cmake --build build --parallel
    
    - name: Test
      run: |
        cd build
        ctest --output-on-failure
    
    - name: Upload Coverage
      if: matrix.build_type == 'Debug' && matrix.compiler == 'gcc'
      uses: codecov/codecov-action@v3
```

## Contributing Guidelines

### Pull Request Process

1. **Fork and Branch**
   ```bash
   git fork https://github.com/yourusername/fauxdb.git
   git checkout -b feature/your-feature-name
   ```

2. **Development**
   - Follow coding standards
   - Add comprehensive tests
   - Update documentation
   - Ensure CI passes

3. **Commit Messages**
   ```
   feat: add aggregation pipeline support
   
   - Implement $match, $project, $group stages
   - Add comprehensive tests for pipeline translation
   - Update documentation with examples
   
   Closes #123
   ```

4. **Pull Request**
   - Provide clear description
   - Reference related issues
   - Include testing instructions
   - Request appropriate reviewers

### Code Review Checklist

- [ ] Code follows style guidelines
- [ ] Comprehensive test coverage
- [ ] Documentation updated
- [ ] No memory leaks or security issues
- [ ] Performance considerations addressed
- [ ] Error handling implemented
- [ ] Logging added where appropriate

## Development Tools

### Useful Scripts

```bash
# tools/scripts/format-code.sh
#!/bin/bash
find src include tests -name "*.cpp" -o -name "*.hpp" | \
    xargs clang-format -i

# tools/scripts/run-tests.sh
#!/bin/bash
cd build-dev
make -j$(nproc) && ctest --output-on-failure

# tools/scripts/check-memory.sh
#!/bin/bash
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
         ./build-dev/fauxdb --config config/fauxdb-dev.conf
```

### Development Docker Setup

```dockerfile
# tools/docker/Dockerfile.dev
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake git gdb valgrind \
    libpq-dev libmongoc-1.0-dev nlohmann-json3-dev \
    clang-format clang-tidy cppcheck

WORKDIR /workspace
COPY . .

RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON .. && \
    make -j$(nproc)

CMD ["./build/fauxdb", "--config", "config/fauxdb-dev.conf"]
```

## Next Steps

- [Installation Guide](installation.md) - Set up development environment
- [Configuration Guide](configuration.md) - Configure development settings
- [Architecture Overview](architecture.md) - Understand system design
- [Performance Tuning](performance.md) - Optimize for development
