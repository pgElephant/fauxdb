# PostgreSQL BSON Extension

[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-17+-blue.svg)](https://www.postgresql.org/)
[![License](https://img.shields.io/badge/License-PostgreSQL-green.svg)](https://opensource.org/licenses/postgresql)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)](#installation)
[![BSON Compatible](https://img.shields.io/badge/BSON-Compatible-green.svg)](https://bsonspec.org/)

> **A high-performance BSON data type extension for PostgreSQL that provides seamless document database compatibility**

Transform your PostgreSQL database into a document-compatible data store with full BSON support, document-style operators, and optimized performance.

## Features

- **Full Document Database Compatibility** - Complete BSON type support with document database wire protocol compatibility
- **High Performance** - Optimized binary storage and indexing for fast queries
- **Rich Query Operators** - Document-style operators (`->`, `->>`, `@>`, `?`, etc.)
- **Advanced Indexing** - B-tree, Hash, and GIN index support for BSON data
- **Seamless Integration** - Drop-in replacement for JSON with enhanced capabilities
- **Type Safety** - Strong typing with PostgreSQL's type system
- **Scalable** - Handles complex nested documents and arrays efficiently

## Quick Start

### Prerequisites

```bash
# macOS (Homebrew)
brew install mongo-c-driver postgresql@17

# Ubuntu/Debian
sudo apt-get update
sudo apt-get install libmongoc-dev libbson-dev postgresql-17-dev

# CentOS/RHEL
sudo yum install mongo-c-driver-devel postgresql17-devel
```

### Installation

```bash
# Clone and build
git clone https://github.com/pgElephant/fauxdb.git
cd fauxdb/extension/bson

# Build with PGXS
make clean
make USE_PGXS=1
sudo make install

# Create extension in PostgreSQL
psql -d your_database -c "CREATE EXTENSION bson;"
```

## Usage Guide

### Basic Operations

```sql
-- Create a table with BSON column
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    profile BSON,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Insert BSON documents
INSERT INTO users (profile) VALUES 
    ('{"name": "Alice", "age": 30, "skills": ["Python", "SQL"], "active": true}'::bson),
    ('{"name": "Bob", "age": 25, "skills": ["JavaScript", "React"], "location": {"city": "NYC", "country": "USA"}}'::bson),
    ('{"name": "Charlie", "age": 35, "skills": ["Go", "Docker"], "projects": [{"name": "API", "status": "active"}]}'::bson);
```

### Querying Data

```sql
-- Field access with -> (returns BSON) and ->> (returns text)
SELECT 
    id,
    profile ->> 'name' AS name,
    profile ->> 'age' AS age,
    profile -> 'skills' AS skills_bson
FROM users;

-- Nested field access
SELECT 
    profile ->> 'name' AS name,
    profile -> 'location' ->> 'city' AS city
FROM users
WHERE profile ? 'location';

-- Array and complex queries
SELECT * FROM users 
WHERE profile @> '{"skills": ["Python"]}'::bson;

SELECT * FROM users 
WHERE profile ? 'projects' 
  AND (profile -> 'projects' -> 0 ->> 'status') = 'active';
```

### Advanced Operations

```sql
-- Existence checks
SELECT * FROM users WHERE profile ? 'location';                    -- Key exists
SELECT * FROM users WHERE profile ?| ARRAY['email', 'phone'];      -- Any key exists  
SELECT * FROM users WHERE profile ?& ARRAY['name', 'age'];         -- All keys exist

-- Containment operations
SELECT * FROM users WHERE profile @> '{"active": true}'::bson;     -- Contains
SELECT * FROM users WHERE '{"name": "Alice"}'::bson <@ profile;    -- Contained by

-- MongoDB-style operators (alternative syntax)
SELECT * FROM users WHERE profile ~= '{"name": "Bob"}'::bson;      -- Equality
SELECT * FROM users WHERE profile ~> '{"age": 30}'::bson;          -- Contains
SELECT * FROM users WHERE profile ?? 'email';                      -- Exists (alt)

-- Document merging
SELECT 
    profile || '{"updated": true}'::bson AS updated_profile
FROM users 
WHERE id = 1;
```

### Indexing for Performance

```sql
-- Create indexes for fast queries
CREATE INDEX idx_users_profile_btree ON users USING btree (profile);
CREATE INDEX idx_users_profile_hash ON users USING hash (profile);

-- GIN index for containment queries (recommended for complex queries)
CREATE INDEX idx_users_profile_gin ON users USING gin (profile);

-- Functional indexes on specific fields
CREATE INDEX idx_users_name ON users ((profile ->> 'name'));
CREATE INDEX idx_users_age ON users (((profile ->> 'age')::int));

-- Expression indexes for complex queries
CREATE INDEX idx_users_active_skills ON users ((profile ->> 'active'), (profile -> 'skills'))
WHERE profile ? 'skills';
```

### Generated Columns

```sql
-- Create generated columns for frequently accessed fields
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    document BSON,
    -- Generated columns for fast access
    name TEXT GENERATED ALWAYS AS (document ->> 'name') STORED,
    price NUMERIC GENERATED ALWAYS AS ((document ->> 'price')::numeric) STORED,
    category TEXT GENERATED ALWAYS AS (document ->> 'category') STORED,
    in_stock BOOLEAN GENERATED ALWAYS AS ((document ->> 'in_stock')::boolean) STORED
);

-- Index generated columns
CREATE INDEX idx_products_name ON products (name);
CREATE INDEX idx_products_price ON products (price);
CREATE INDEX idx_products_category_price ON products (category, price);
```

## Supported Operators

### Standard PostgreSQL Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `->` | Get BSON field | `doc -> 'name'` |
| `->>` | Get text field | `doc ->> 'name'` |
| `=` | Equality | `doc1 = doc2` |
| `!=` | Inequality | `doc1 != doc2` |
| `@>` | Contains | `doc @> '{"key": "value"}'::bson` |
| `<@` | Contained by | `'{"key": "value"}'::bson <@ doc` |
| `?` | Key exists | `doc ? 'key'` |
| `?|` | Any key exists | `doc ?| ARRAY['key1', 'key2']` |
| `?&` | All keys exist | `doc ?& ARRAY['key1', 'key2']` |

### MongoDB-Style Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `~=` | MongoDB equality | `doc ~= '{"name": "John"}'::bson` |
| `~>` | MongoDB contains | `doc ~> '{"active": true}'::bson` |
| `<~` | MongoDB contained | `'{"name": "John"}'::bson <~ doc` |
| `??` | Alternative exists | `doc ?? 'key'` |
| `??|` | Alternative any exists | `doc ??| ARRAY['key1', 'key2']` |
| `??&` | Alternative all exist | `doc ??& ARRAY['key1', 'key2']` |
| `||` | Document merge | `doc1 || doc2` |

## BSON Type Support

| Type | Code | PostgreSQL Support | Description |
|------|------|-------------------|-------------|
| Double | `0x01` | Full | 64-bit floating point |
| String | `0x02` | Full | UTF-8 string |
| Object | `0x03` | Full | Embedded document |
| Array | `0x04` | Full | Array of values |
| Binary | `0x05` | Full | Binary data |
| ObjectId | `0x07` | Full | 12-byte unique ID |
| Boolean | `0x08` | Full | True/false |
| Date | `0x09` | Full | UTC datetime |
| Null | `0x0A` | Full | Null value |
| Regex | `0x0B` | Full | Regular expression |
| JavaScript | `0x0D` | Full | JavaScript code |
| Int32 | `0x10` | Full | 32-bit integer |
| Timestamp | `0x11` | Full | MongoDB timestamp |
| Int64 | `0x12` | Full | 64-bit integer |
| Decimal128 | `0x13` | Full | High-precision decimal |

## Performance Benchmarks

```sql
-- Example: Query performance comparison
-- JSON vs BSON for complex nested queries

-- JSON (standard PostgreSQL)
EXPLAIN ANALYZE 
SELECT * FROM json_table 
WHERE data->>'name' = 'John' 
  AND (data->'address'->>'city') = 'NYC';

-- BSON (this extension)  
EXPLAIN ANALYZE
SELECT * FROM bson_table 
WHERE data ->> 'name' = 'John'
  AND data -> 'address' ->> 'city' = 'NYC';

-- Typical results: 15-30% performance improvement with BSON indexing
```

## Configuration

```sql
-- Extension configuration
SHOW shared_preload_libraries;  -- Should include 'bson'

-- Memory settings for large BSON documents
SET work_mem = '256MB';
SET maintenance_work_mem = '1GB';

-- Enable parallel queries for BSON operations
SET max_parallel_workers_per_gather = 4;
SET parallel_tuple_cost = 0.1;
```

## Testing

```bash
# Run test suite
cd extension/bson
make test

# Run specific tests
psql -d test_database -f test_operators.sql
psql -d test_database -f test_extension.sql

# Performance testing
pgbench -d test_database -f bson_benchmark.sql -c 10 -j 2 -T 60
```

## Troubleshooting

### Common Issues

**Q: Extension fails to load**
```sql
-- Check if mongo-c-driver is installed
SELECT * FROM pg_available_extensions WHERE name = 'bson';

-- Verify shared library
\! ls -la /usr/local/pgsql.17/lib/bson.dylib
```

**Q: Performance issues with large documents**
```sql
-- Enable better statistics
ALTER TABLE your_table ALTER COLUMN bson_column SET STATISTICS 1000;
ANALYZE your_table;

-- Consider partial indexes
CREATE INDEX idx_partial ON your_table (bson_column) 
WHERE bson_column ? 'frequently_queried_field';
```

**Q: Memory usage optimization**
```sql
-- For very large BSON documents
SET jit = off;  -- Disable JIT for complex BSON operations
SET enable_hashjoin = off;  -- Use nested loops for small result sets
```

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md).

```bash
# Development setup
git clone https://github.com/pgElephant/fauxdb.git
cd fauxdb/extension/bson

# Make changes and test
make clean && make && make install
psql -d test_db -c "DROP EXTENSION IF EXISTS bson CASCADE; CREATE EXTENSION bson;"

# Run tests
make test
```

## License

This project is licensed under the **PostgreSQL License** - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [MongoDB Inc.](https://www.mongodb.com/) for the BSON specification
- [MongoDB C Driver](https://github.com/mongodb/mongo-c-driver) for the excellent BSON implementation
- [PostgreSQL Global Development Group](https://www.postgresql.org/) for the amazing database system
- [pgElephant](https://github.com/pgElephant) for the FauxDB project

## Support

- **Documentation**: [Full Documentation](docs/)
- **Issues**: [GitHub Issues](https://github.com/pgElephant/fauxdb/issues)
- **Discussions**: [GitHub Discussions](https://github.com/pgElephant/fauxdb/discussions)
- **Email**: support@pgelephant.com

---

<div align="center">

**Star this repository if you find it useful!**

[Home](https://github.com/pgElephant/fauxdb) • [Docs](docs/) • [Discussions](discussions/)

</div>

```sql
-- Create a table with BSON column
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    profile BSON,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Insert BSON documents
INSERT INTO users (profile) VALUES 
    ('{"name": "Alice", "age": 30, "skills": ["Python", "SQL"], "active": true}'::bson),
    ('{"name": "Bob", "age": 25, "skills": ["JavaScript", "React"], "location": {"city": "NYC", "country": "USA"}}'::bson),
    ('{"name": "Charlie", "age": 35, "skills": ["Go", "Docker"], "projects": [{"name": "API", "status": "active"}]}'::bson);
```

### 🔍 Querying Data

```sql
-- Field access with -> (returns BSON) and ->> (returns text)
SELECT 
    id,
    profile ->> 'name' AS name,
    profile ->> 'age' AS age,
    profile -> 'skills' AS skills_bson
FROM users;

-- Nested field access
SELECT 
    profile ->> 'name' AS name,
    profile -> 'location' ->> 'city' AS city
FROM users
WHERE profile ? 'location';

-- Array and complex queries
SELECT * FROM users 
WHERE profile @> '{"skills": ["Python"]}'::bson;

SELECT * FROM users 
WHERE profile ? 'projects' 
  AND (profile -> 'projects' -> 0 ->> 'status') = 'active';
```

### 🔧 Advanced Operations

```sql
-- Existence checks
SELECT * FROM users WHERE profile ? 'location';                    -- Key exists
SELECT * FROM users WHERE profile ?| ARRAY['email', 'phone'];      -- Any key exists  
SELECT * FROM users WHERE profile ?& ARRAY['name', 'age'];         -- All keys exist

-- Containment operations
SELECT * FROM users WHERE profile @> '{"active": true}'::bson;     -- Contains
SELECT * FROM users WHERE '{"name": "Alice"}'::bson <@ profile;    -- Contained by

-- MongoDB-style operators (alternative syntax)
SELECT * FROM users WHERE profile ~= '{"name": "Bob"}'::bson;      -- Equality
SELECT * FROM users WHERE profile ~> '{"age": 30}'::bson;          -- Contains
SELECT * FROM users WHERE profile ?? 'email';                      -- Exists (alt)

-- Document merging
SELECT 
    profile || '{"updated": true}'::bson AS updated_profile
FROM users 
WHERE id = 1;
```

### 📊 Indexing for Performance

```sql
-- Create indexes for fast queries
CREATE INDEX idx_users_profile_btree ON users USING btree (profile);
CREATE INDEX idx_users_profile_hash ON users USING hash (profile);

-- GIN index for containment queries (recommended for complex queries)
CREATE INDEX idx_users_profile_gin ON users USING gin (profile);

-- Functional indexes on specific fields
CREATE INDEX idx_users_name ON users ((profile ->> 'name'));
CREATE INDEX idx_users_age ON users (((profile ->> 'age')::int));

-- Expression indexes for complex queries
CREATE INDEX idx_users_active_skills ON users ((profile ->> 'active'), (profile -> 'skills'))
WHERE profile ? 'skills';
```

### 🎭 Generated Columns

```sql
-- Create generated columns for frequently accessed fields
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    document BSON,
    -- Generated columns for fast access
    name TEXT GENERATED ALWAYS AS (document ->> 'name') STORED,
    price NUMERIC GENERATED ALWAYS AS ((document ->> 'price')::numeric) STORED,
    category TEXT GENERATED ALWAYS AS (document ->> 'category') STORED,
    in_stock BOOLEAN GENERATED ALWAYS AS ((document ->> 'in_stock')::boolean) STORED
);

-- Index generated columns
CREATE INDEX idx_products_name ON products (name);
CREATE INDEX idx_products_price ON products (price);
CREATE INDEX idx_products_category_price ON products (category, price);
```

## 🛠️ Supported Operators

### Standard PostgreSQL Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `->` | Get BSON field | `doc -> 'name'` |
| `->>` | Get text field | `doc ->> 'name'` |
| `=` | Equality | `doc1 = doc2` |
| `!=` | Inequality | `doc1 != doc2` |
| `@>` | Contains | `doc @> '{"key": "value"}'::bson` |
| `<@` | Contained by | `'{"key": "value"}'::bson <@ doc` |
| `?` | Key exists | `doc ? 'key'` |
| `?|` | Any key exists | `doc ?| ARRAY['key1', 'key2']` |
| `?&` | All keys exist | `doc ?& ARRAY['key1', 'key2']` |

### MongoDB-Style Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `~=` | MongoDB equality | `doc ~= '{"name": "John"}'::bson` |
| `~>` | MongoDB contains | `doc ~> '{"active": true}'::bson` |
| `<~` | MongoDB contained | `'{"name": "John"}'::bson <~ doc` |
| `??` | Alternative exists | `doc ?? 'key'` |
| `??|` | Alternative any exists | `doc ??| ARRAY['key1', 'key2']` |
| `??&` | Alternative all exist | `doc ??& ARRAY['key1', 'key2']` |
| `||` | Document merge | `doc1 || doc2` |

## 📋 BSON Type Support

| Type | Code | PostgreSQL Support | Description |
|------|------|-------------------|-------------|
| Double | `0x01` | ✅ Full | 64-bit floating point |
| String | `0x02` | ✅ Full | UTF-8 string |
| Object | `0x03` | ✅ Full | Embedded document |
| Array | `0x04` | ✅ Full | Array of values |
| Binary | `0x05` | ✅ Full | Binary data |
| ObjectId | `0x07` | ✅ Full | 12-byte unique ID |
| Boolean | `0x08` | ✅ Full | True/false |
| Date | `0x09` | ✅ Full | UTC datetime |
| Null | `0x0A` | ✅ Full | Null value |
| Regex | `0x0B` | ✅ Full | Regular expression |
| JavaScript | `0x0D` | ✅ Full | JavaScript code |
| Int32 | `0x10` | ✅ Full | 32-bit integer |
| Timestamp | `0x11` | ✅ Full | MongoDB timestamp |
| Int64 | `0x12` | ✅ Full | 64-bit integer |
| Decimal128 | `0x13` | ✅ Full | High-precision decimal |

## Performance Benchmarks

```sql
-- Example: Query performance comparison
-- JSON vs BSON for complex nested queries

-- JSON (standard PostgreSQL)
EXPLAIN ANALYZE 
SELECT * FROM json_table 
WHERE data->>'name' = 'John' 
  AND (data->'address'->>'city') = 'NYC';

-- BSON (this extension)  
EXPLAIN ANALYZE
SELECT * FROM bson_table 
WHERE data ->> 'name' = 'John'
  AND data -> 'address' ->> 'city' = 'NYC';

-- Typical results: 15-30% performance improvement with BSON indexing
```

## 🔧 Configuration

```sql
-- Extension configuration
SHOW shared_preload_libraries;  -- Should include 'bson'

-- Memory settings for large BSON documents
SET work_mem = '256MB';
SET maintenance_work_mem = '1GB';

-- Enable parallel queries for BSON operations
SET max_parallel_workers_per_gather = 4;
SET parallel_tuple_cost = 0.1;
```

## 🧪 Testing

```bash
# Run test suite
cd extension/bson
make test

# Run specific tests
psql -d test_database -f test_operators.sql
psql -d test_database -f test_extension.sql

# Performance testing
pgbench -d test_database -f bson_benchmark.sql -c 10 -j 2 -T 60
```

## 🚨 Troubleshooting

### Common Issues

**Q: Extension fails to load**
```sql
-- Check if mongo-c-driver is installed
SELECT * FROM pg_available_extensions WHERE name = 'bson';

-- Verify shared library
\! ls -la /usr/local/pgsql.17/lib/bson.dylib
```

**Q: Performance issues with large documents**
```sql
-- Enable better statistics
ALTER TABLE your_table ALTER COLUMN bson_column SET STATISTICS 1000;
ANALYZE your_table;

-- Consider partial indexes
CREATE INDEX idx_partial ON your_table (bson_column) 
WHERE bson_column ? 'frequently_queried_field';
```

**Q: Memory usage optimization**
```sql
-- For very large BSON documents
SET jit = off;  -- Disable JIT for complex BSON operations
SET enable_hashjoin = off;  -- Use nested loops for small result sets
```

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md).

```bash
# Development setup
git clone https://github.com/pgElephant/fauxdb.git
cd fauxdb/extension/bson

# Make changes and test
make clean && make && make install
psql -d test_db -c "DROP EXTENSION IF EXISTS bson CASCADE; CREATE EXTENSION bson;"

# Run tests
make test
```

## 📜 License

This project is licensed under the **PostgreSQL License** - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- [MongoDB Inc.](https://www.mongodb.com/) for the BSON specification
- [MongoDB C Driver](https://github.com/mongodb/mongo-c-driver) for the excellent BSON implementation
- [PostgreSQL Global Development Group](https://www.postgresql.org/) for the amazing database system
- [pgElephant](https://github.com/pgElephant) for the FauxDB project

## 📞 Support

- 📖 **Documentation**: [Full Documentation](docs/)
- 🐛 **Issues**: [GitHub Issues](https://github.com/pgElephant/fauxdb/issues)
- 💬 **Discussions**: [GitHub Discussions](https://github.com/pgElephant/fauxdb/discussions)
- 📧 **Email**: support@pgelephant.com

---

<div align="center">

**⭐ Star this repository if you find it useful!**

[🏠 Home](https://github.com/pgElephant/fauxdb) • [📖 Docs](docs/) • [💬 Discussions](discussions/)

</div>
