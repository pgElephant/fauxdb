# DocumentDB Compatibility in FauxDB

## Overview

FauxDB provides MongoDB-compatible API that translates MongoDB queries to PostgreSQL JSONB operations. This document explains our approach to DocumentDB compatibility, similar to how FerretDB handles MongoDB compatibility.

## Architecture

```
MongoDB Client (mongosh, drivers) 
    ↓ (MongoDB Wire Protocol)
FauxDB Server (Port 27018)
    ↓ (Query Translation)
PostgreSQL with JSONB (Port 5432)
```

## DocumentDB vs MongoDB Compatibility

### What is DocumentDB?

Amazon DocumentDB is a MongoDB-compatible database service. It implements the MongoDB 3.6 and 4.0 wire protocol, making it compatible with existing MongoDB applications and drivers.

### FauxDB's Approach

FauxDB follows a similar strategy to FerretDB:

1. **Wire Protocol Compatibility**: Implements MongoDB wire protocol to accept connections from MongoDB clients
2. **Query Translation**: Translates MongoDB queries to PostgreSQL JSONB operations
3. **Schema-less Design**: Stores documents in PostgreSQL JSONB columns
4. **Index Support**: Leverages PostgreSQL's GIN indexes on JSONB for performance

## Supported Operations

### CRUD Operations
- `insertOne()`, `insertMany()`
- `find()`, `findOne()`
- `updateOne()`, `updateMany()`, `replaceOne()`
- `deleteOne()`, `deleteMany()`
- `countDocuments()`, `estimatedDocumentCount()`

### Query Operators
- Comparison: `$eq`, `$ne`, `$gt`, `$gte`, `$lt`, `$lte`, `$in`, `$nin`
- Logical: `$and`, `$or`, `$nor`, `$not`
- Element: `$exists`, `$type`
- Array: `$all`, `$elemMatch`, `$size`
- Evaluation: `$expr`, `$jsonSchema`, `$mod`, `$regex`, `$text`, `$where`

### Aggregation Pipeline
- `$match`, `$project`, `$group`, `$sort`, `$limit`, `$skip`
- `$unwind`, `$lookup`, `$addFields`, `$replaceRoot`
- `$count`, `$sum`, `$avg`, `$min`, `$max`, `$first`, `$last`

### Indexing
- Single field indexes
- Compound indexes
- Partial indexes
- Sparse indexes
- TTL indexes (via PostgreSQL scheduled jobs)

## PostgreSQL Extensions Used

### Core Extensions
```sql
-- Required for JSONB operations
CREATE EXTENSION IF NOT EXISTS btree_gin;
CREATE EXTENSION IF NOT EXISTS btree_gist;
CREATE EXTENSION IF NOT EXISTS pg_trgm;

-- For advanced scripting (if needed)
CREATE EXTENSION IF NOT EXISTS plpython3u;
```

### JSONB Optimization
- **GIN Indexes**: For efficient JSONB queries
- **Expression Indexes**: For specific field queries
- **Partial Indexes**: For conditional indexing

## Configuration

### FauxDB Configuration (`config/fauxdb.toml`)
```toml
[server]
host = "127.0.0.1"
port = 27018  # MongoDB-compatible port
max_connections = 1000

[database]
uri = "postgresql://user:password@localhost:5432/fauxdb"
max_connections = 10
enable_jsonb_extensions = true

[authentication]
enabled = false  # For development
default_auth_method = "SCRAM-SHA-256"
```

### PostgreSQL Configuration
```sql
-- Enable JSONB extensions
CREATE EXTENSION IF NOT EXISTS btree_gin;
CREATE EXTENSION IF NOT EXISTS btree_gist;
CREATE EXTENSION IF NOT EXISTS pg_trgm;

-- Create database and user
CREATE USER fauxdb_user WITH PASSWORD 'your_password';
CREATE DATABASE fauxdb;
GRANT ALL PRIVILEGES ON DATABASE fauxdb TO fauxdb_user;
```

## Data Storage Strategy

### Collection Storage
Each MongoDB collection is stored as a PostgreSQL table with this schema:

```sql
CREATE TABLE collection_name (
    id SERIAL PRIMARY KEY,
    _id TEXT UNIQUE NOT NULL,
    document JSONB NOT NULL,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Indexes for performance
CREATE INDEX idx_collection_document_gin ON collection_name USING GIN (document);
CREATE INDEX idx_collection_id ON collection_name (_id);
```

### Document ID Handling
- MongoDB's `_id` field is stored as a separate column for fast lookups
- Auto-generated ObjectIds are handled by FauxDB
- Custom `_id` values are supported

## Performance Considerations

### Indexing Strategy
1. **Automatic GIN Index**: Created on all JSONB columns
2. **Field-specific Indexes**: For frequently queried fields
3. **Compound Indexes**: For multi-field queries

### Query Optimization
1. **Query Planning**: Analyzes MongoDB queries to optimize PostgreSQL execution
2. **Index Usage**: Leverages PostgreSQL's query planner for JSONB
3. **Connection Pooling**: Maintains connection pools to PostgreSQL

## Limitations and Differences

### Not Supported (Yet)
- GridFS (file storage)
- Change streams
- Transactions (multi-document)
- Some aggregation operators
- Geospatial queries (basic support planned)

### Differences from MongoDB
1. **ACID Compliance**: Full ACID transactions (PostgreSQL)
2. **SQL Access**: Can query data directly with SQL
3. **Backup/Restore**: Uses PostgreSQL tools
4. **Replication**: PostgreSQL streaming replication

## Migration from MongoDB/DocumentDB

### Option 1: Application-Level Migration
```bash
# Export from MongoDB
mongodump --uri="mongodb://source-server:27017/mydb"

# Import to FauxDB (same commands)
mongorestore --uri="mongodb://localhost:27018/mydb"
```

### Option 2: Application Code Changes
No changes needed! FauxDB uses the same MongoDB drivers and connection strings.

```javascript
// Same code works with FauxDB
const { MongoClient } = require('mongodb');
const client = new MongoClient('mongodb://localhost:27018');
```

## Testing and Validation

### Compatibility Tests
```bash
# Run the test suite
./test.sh

# Test specific operations
mongosh mongodb://localhost:27018/test --eval "
  db.users.insertOne({name: 'John', age: 30});
  db.users.find().pretty();
"
```

### Performance Testing
```bash
# Benchmark against MongoDB
./benchmark.sh

# Compare with PostgreSQL direct queries
psql -d fauxdb -c "SELECT * FROM users WHERE document->>'name' = 'John';"
```

## Troubleshooting

### Common Issues

1. **Connection Refused**
   ```bash
   # Check if FauxDB is running
   netstat -tlnp | grep 27018
   
   # Check PostgreSQL connection
   psql -h localhost -U fauxdb_user -d fauxdb
   ```

2. **Query Performance Issues**
   ```sql
   -- Check index usage
   EXPLAIN ANALYZE SELECT * FROM collection_name WHERE document @> '{"field": "value"}';
   
   -- Create specific indexes
   CREATE INDEX idx_field ON collection_name USING GIN ((document->'field'));
   ```

3. **Authentication Issues**
   ```bash
   # Check PostgreSQL user permissions
   sudo -u postgres psql -c "\du"
   
   # Verify connection string in config
   grep -A 5 "\[database\]" config/fauxdb.toml
   ```

## Development and Contributing

### Adding New Features
1. Implement MongoDB wire protocol handlers
2. Add query translation logic
3. Update PostgreSQL schema if needed
4. Add tests and documentation

### Testing New Features
```bash
# Run unit tests
cargo test

# Run integration tests
cargo test --test integration

# Test with real MongoDB client
mongosh mongodb://localhost:27018/test
```

## References

- [MongoDB Wire Protocol Specification](https://docs.mongodb.com/manual/reference/mongodb-wire-protocol/)
- [PostgreSQL JSONB Documentation](https://www.postgresql.org/docs/current/datatype-json.html)
- [FerretDB Architecture](https://docs.ferretdb.io/architecture/)
- [Amazon DocumentDB Compatibility](https://docs.aws.amazon.com/documentdb/latest/developerguide/mongo-apis.html)

## License

FauxDB is licensed under the MIT License. See LICENSE file for details.
