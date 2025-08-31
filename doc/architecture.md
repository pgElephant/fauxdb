# Architecture Overview

## System Architecture

FauxDB is designed as a translation layer between MongoDB-compatible clients and PostgreSQL databases. It implements the MongoDB wire protocol while leveraging PostgreSQL's robust ACID guarantees and advanced SQL capabilities.

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   MongoDB       │    │                 │    │   PostgreSQL    │
│   Clients       │◄──►│     FauxDB      │◄──►│   Database      │
│  (mongosh,      │    │                 │    │                 │
│   drivers)      │    │                 │    │                 │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Core Components

### 1. Network Layer

#### Network Manager
- **Purpose**: Handles TCP connections and network I/O
- **Responsibilities**:
  - Accept client connections
  - Manage connection lifecycle
  - Implement MongoDB wire protocol framing
  - Handle connection pooling and multiplexing

```cpp
class NetworkManager {
    void startServer(int port, const std::string& bindAddress);
    void handleConnection(int clientSocket);
    void processMessage(const std::vector<uint8_t>& message);
};
```

#### Connection Handling
- Non-blocking I/O with epoll/kqueue
- Per-connection state management
- Graceful connection shutdown
- Connection timeout handling

### 2. Protocol Layer

#### Protocol Handler
- **Purpose**: Parse and generate MongoDB wire protocol messages
- **Responsibilities**:
  - Parse incoming MongoDB commands
  - Generate appropriate responses
  - Handle various operation types (OP_QUERY, OP_INSERT, etc.)
  - Manage request/response correlation

```cpp
class ProtocolHandler {
    std::vector<uint8_t> handleQuery(const QueryMessage& query);
    std::vector<uint8_t> handleInsert(const InsertMessage& insert);
    std::vector<uint8_t> handleUpdate(const UpdateMessage& update);
    std::vector<uint8_t> handleDelete(const DeleteMessage& delete);
};
```

#### Message Types
- **OP_QUERY**: Find operations and commands
- **OP_INSERT**: Document insertion
- **OP_UPDATE**: Document updates
- **OP_DELETE**: Document deletion
- **OP_MSG**: Modern MongoDB commands (MongoDB 3.6+)

### 3. Query Translation Layer

#### Query Translator
- **Purpose**: Convert MongoDB queries to PostgreSQL SQL
- **Responsibilities**:
  - Parse MongoDB query syntax
  - Generate equivalent SQL queries
  - Handle operator translation
  - Optimize query structure

```cpp
class QueryTranslator {
    std::string translateFind(const BSONObj& filter, const BSONObj& projection);
    std::string translateInsert(const std::string& collection, const BSONObj& document);
    std::string translateUpdate(const std::string& collection, const BSONObj& filter, const BSONObj& update);
    std::string translateDelete(const std::string& collection, const BSONObj& filter);
    std::string translateAggregate(const std::string& collection, const std::vector<BSONObj>& pipeline);
};
```

#### Translation Strategies

**Document Storage**:
```sql
-- Collections are stored as PostgreSQL tables
CREATE TABLE users (
    _id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    document JSONB NOT NULL,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Indexes on JSONB fields for performance
CREATE INDEX idx_users_email ON users USING BTREE ((document->>'email'));
CREATE INDEX idx_users_age ON users USING BTREE (((document->>'age')::int));
CREATE INDEX idx_users_document_gin ON users USING GIN (document);
```

**Query Translation Examples**:

```javascript
// MongoDB Query
db.users.find({age: {$gte: 25}, status: "active"})

// Translated SQL
SELECT document FROM users 
WHERE (document->>'age')::int >= 25 
  AND document->>'status' = 'active';
```

```javascript
// MongoDB Aggregation
db.users.aggregate([
  {$match: {age: {$gte: 18}}},
  {$group: {_id: "$department", count: {$sum: 1}}},
  {$sort: {count: -1}}
])

// Translated SQL
SELECT document->>'department' as _id, COUNT(*) as count
FROM users 
WHERE (document->>'age')::int >= 18
GROUP BY document->>'department'
ORDER BY count DESC;
```

### 4. Database Layer

#### Database Interface
- **Purpose**: Abstract database operations
- **Responsibilities**:
  - Execute SQL queries on PostgreSQL
  - Manage database connections
  - Handle transactions
  - Convert SQL results to MongoDB format

```cpp
class DatabaseInterface {
    virtual DbResult executeQuery(const std::string& sql) = 0;
    virtual DbResult executeTransaction(const std::vector<std::string>& queries) = 0;
    virtual bool isConnected() = 0;
};

class PostgreSQLDatabase : public DatabaseInterface {
    PGconn* connection;
    DbResult executeQuery(const std::string& sql) override;
    DbResult executeTransaction(const std::vector<std::string>& queries) override;
};
```

#### Connection Pooling
- **Purpose**: Efficiently manage database connections
- **Features**:
  - Connection reuse and pooling
  - Automatic connection health checks
  - Load balancing across connections
  - Connection timeout handling

```cpp
class DatabasePool {
    std::queue<std::unique_ptr<DatabaseConnection>> available_connections;
    std::vector<std::unique_ptr<DatabaseConnection>> all_connections;
    
    std::unique_ptr<DatabaseConnection> getConnection();
    void returnConnection(std::unique_ptr<DatabaseConnection> conn);
    void healthCheck();
};
```

### 5. Data Processing Layer

#### BSON Processing
- **Purpose**: Handle BSON encoding/decoding
- **Responsibilities**:
  - Parse BSON documents from client messages
  - Generate BSON responses
  - Convert between BSON and JSON formats
  - Handle MongoDB-specific data types

```cpp
class BSONProcessor {
    BSONObj parseBSONDocument(const std::vector<uint8_t>& data, size_t& offset);
    std::vector<uint8_t> generateBSONResponse(const BSONObj& document);
    std::string bsonToJSON(const BSONObj& bson);
    BSONObj jsonToBSON(const std::string& json);
};
```

#### Result Processing
- **Purpose**: Convert PostgreSQL results to MongoDB format
- **Responsibilities**:
  - Transform SQL result sets to BSON documents
  - Handle data type conversions
  - Implement MongoDB result semantics
  - Generate appropriate response metadata

### 6. Configuration and Management

#### Configuration Manager
- **Purpose**: Handle server configuration
- **Features**:
  - File-based configuration
  - Environment variable support
  - Runtime configuration updates
  - Configuration validation

```cpp
class ConfigManager {
    ServerConfig server_config;
    DatabaseConfig database_config;
    LoggingConfig logging_config;
    
    bool loadFromFile(const std::string& configFile);
    bool loadFromEnvironment();
    void validate();
};
```

#### Server Orchestrator
- **Purpose**: Coordinate all system components
- **Responsibilities**:
  - Initialize and start components
  - Handle graceful shutdown
  - Manage component lifecycle
  - Coordinate error handling

```cpp
class ServerOrchestrator {
    std::unique_ptr<NetworkManager> network_manager;
    std::unique_ptr<ProtocolHandler> protocol_handler;
    std::unique_ptr<QueryTranslator> query_translator;
    std::unique_ptr<DatabasePool> database_pool;
    
    void initialize();
    void start();
    void stop();
    void handleSignal(int signal);
};
```

## Data Flow

### Query Processing Flow

1. **Connection Establishment**
   ```
   Client → NetworkManager → Accept Connection → Create Session
   ```

2. **Message Reception**
   ```
   Client Message → NetworkManager → Parse Wire Protocol → Extract MongoDB Command
   ```

3. **Query Translation**
   ```
   MongoDB Query → QueryTranslator → Parse BSON → Generate SQL → Validate Query
   ```

4. **Database Execution**
   ```
   SQL Query → DatabasePool → Get Connection → Execute on PostgreSQL → Return Results
   ```

5. **Response Generation**
   ```
   SQL Results → ProtocolHandler → Convert to BSON → Generate MongoDB Response → Send to Client
   ```

### Detailed Flow Example

```
1. Client sends: db.users.find({age: {$gte: 25}})
   ↓
2. NetworkManager receives wire protocol message
   ↓
3. ProtocolHandler parses OP_QUERY message
   ↓
4. QueryTranslator converts to: 
   SELECT document FROM users WHERE (document->>'age')::int >= 25
   ↓
5. DatabasePool executes SQL on PostgreSQL
   ↓
6. PostgreSQL returns JSONB documents
   ↓
7. ProtocolHandler converts results to BSON
   ↓
8. NetworkManager sends OP_REPLY to client
   ↓
9. Client receives MongoDB-compatible results
```

## Threading Model

### Multi-threaded Architecture

```
Main Thread
├── Signal Handler Thread
├── Network Accept Thread
├── Worker Thread Pool
│   ├── Worker Thread 1 (Connection Handler)
│   ├── Worker Thread 2 (Query Processor)
│   ├── Worker Thread 3 (Database Handler)
│   └── Worker Thread N
└── Management Thread
    ├── Health Check Thread
    ├── Statistics Thread
    └── Cleanup Thread
```

### Thread Responsibilities

- **Main Thread**: Initialization and coordination
- **Network Threads**: Accept connections and handle I/O
- **Worker Threads**: Process queries and execute database operations
- **Management Threads**: Health checks, monitoring, and maintenance

### Synchronization

- **Lock-free queues** for message passing
- **Atomic operations** for statistics and counters
- **Mutex protection** for shared resources
- **Connection pools** with thread-safe access

## Memory Management

### Resource Management Strategy

1. **RAII Pattern**: Automatic resource cleanup
2. **Smart Pointers**: Automatic memory management
3. **Connection Pooling**: Reuse expensive resources
4. **Buffer Management**: Efficient memory allocation

### Memory Layout

```
Process Memory
├── Code Segment (executable)
├── Data Segment (global variables)
├── Heap (dynamic allocation)
│   ├── Connection Objects
│   ├── Message Buffers
│   ├── Query Objects
│   └── Result Sets
└── Stack (per thread)
    ├── Local Variables
    ├── Function Calls
    └── Thread Context
```

## Error Handling

### Error Propagation

```
Database Error → DatabaseInterface → QueryProcessor → ProtocolHandler → Client
```

### Error Types

1. **Network Errors**: Connection failures, timeouts
2. **Protocol Errors**: Invalid MongoDB commands
3. **Translation Errors**: Unsupported query features
4. **Database Errors**: PostgreSQL execution failures
5. **System Errors**: Resource exhaustion, permissions

### Error Response Strategy

```cpp
enum class ErrorCode {
    Success = 0,
    NetworkError = 1,
    ProtocolError = 2,
    TranslationError = 3,
    DatabaseError = 4,
    SystemError = 5
};

class ErrorHandler {
    MongoDBError translateError(ErrorCode code, const std::string& message);
    std::vector<uint8_t> generateErrorResponse(uint32_t requestId, const MongoDBError& error);
};
```

## Performance Characteristics

### Throughput Metrics

- **Query Translation**: ~100,000 operations/second
- **Network Handling**: ~50,000 connections/second
- **Database Operations**: Limited by PostgreSQL performance
- **Memory Usage**: ~50MB base + ~1KB per connection

### Optimization Strategies

1. **Connection Pooling**: Reduce connection overhead
2. **Query Caching**: Cache translated queries
3. **Prepared Statements**: Optimize repeated queries
4. **Batch Operations**: Group multiple operations
5. **Index Optimization**: Leverage PostgreSQL indexes

## Scalability Considerations

### Horizontal Scaling

```
Load Balancer
├── FauxDB Instance 1 → PostgreSQL Primary
├── FauxDB Instance 2 → PostgreSQL Primary
└── FauxDB Instance N → PostgreSQL Primary
```

### Vertical Scaling

- **CPU**: Multi-core utilization through threading
- **Memory**: Efficient memory management and pooling
- **I/O**: Non-blocking I/O and connection multiplexing
- **Database**: PostgreSQL performance tuning

### High Availability

- **Health Checks**: Monitor component health
- **Graceful Degradation**: Handle partial failures
- **Circuit Breakers**: Prevent cascade failures
- **Failover Support**: Automatic recovery mechanisms

## Security Architecture

### Security Layers

1. **Network Security**: TLS encryption, IP filtering
2. **Authentication**: MongoDB-compatible auth mechanisms
3. **Authorization**: Role-based access control
4. **Input Validation**: SQL injection prevention
5. **Audit Logging**: Security event tracking

### Data Protection

```
Client ←[TLS]→ FauxDB ←[SSL]→ PostgreSQL
```

- **Encryption in Transit**: TLS/SSL connections
- **Data Sanitization**: Input validation and escaping
- **Access Control**: Database-level permissions
- **Audit Trail**: Comprehensive logging

## Extension Points

### Plugin Architecture

```cpp
class Plugin {
    virtual void initialize() = 0;
    virtual void process(const QueryContext& context) = 0;
    virtual void cleanup() = 0;
};

class PluginManager {
    std::vector<std::unique_ptr<Plugin>> plugins;
    void loadPlugin(const std::string& path);
    void executePlugins(const QueryContext& context);
};
```

### Customization Points

1. **Query Translators**: Custom translation rules
2. **Authentication Providers**: External auth integration
3. **Monitoring Hooks**: Custom metrics collection
4. **Storage Backends**: Alternative database support

## Future Enhancements

### Planned Features

1. **Sharding Support**: Distribute data across multiple PostgreSQL instances
2. **Replica Sets**: Read replica simulation
3. **Change Streams**: Real-time data change notifications
4. **GridFS Support**: Large file storage capabilities
5. **Full-Text Search**: Advanced text search features

### Architecture Evolution

- **Microservices**: Component separation for scalability
- **Cloud Native**: Kubernetes-native deployment
- **Streaming**: Real-time data processing capabilities
- **ML Integration**: Query optimization through machine learning

## Next Steps

- [Performance Tuning](performance.md) - Optimize your deployment
- [Monitoring Setup](monitoring.md) - Set up comprehensive monitoring
- [Development Guide](development.md) - Contributing to FauxDB
- [Deployment Guide](deployment.md) - Production deployment strategies
