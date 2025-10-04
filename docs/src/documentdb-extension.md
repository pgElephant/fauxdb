---
layout: default
title: DocumentDB Extension Installation
parent: Guides
nav_order: 5
description: Installation guide for DocumentDB third-party extension in PostgreSQL
---

# DocumentDB Extension Installation Guide

This guide provides instructions for installing the DocumentDB third-party extension in PostgreSQL to enhance MongoDB compatibility for FauxDB.

## Overview

FauxDB requires a DocumentDB-compatible third-party extension to provide enhanced MongoDB query capabilities on PostgreSQL. This extension enables MongoDB-style operations on JSONB data.

**Note**: The standard PostgreSQL extensions (btree_gin, btree_gist, pg_trgm, plpython3u) are included with PostgreSQL and are installed automatically by the build script. Only the DocumentDB extension is a third-party extension that requires manual installation.

## Prerequisites

### System Requirements
- PostgreSQL 13 or higher
- GCC compiler and development tools
- Make utility
- PostgreSQL development headers

### Verification Commands
```bash
# Check PostgreSQL version
sudo -u postgres psql -c "SELECT version();"

# Verify required tools
gcc --version
make --version
pg_config --version
```

## DocumentDB Extension Installation

### 1. Download DocumentDB Extension

```bash
# Create extensions directory
sudo mkdir -p /opt/postgresql/extensions
cd /opt/postgresql/extensions

# Download DocumentDB extension source
# Note: Replace with actual DocumentDB extension repository
sudo git clone https://github.com/aws/documentdb-compatibility-layer.git documentdb
cd documentdb
```

### 2. Install Dependencies

```bash
# Install required packages (Rocky Linux/RHEL/CentOS)
sudo dnf install -y \
    postgresql-devel \
    postgresql-server-devel \
    gcc \
    make \
    git

# For Ubuntu/Debian
sudo apt install -y \
    postgresql-server-dev-all \
    build-essential \
    git
```

### 3. Build and Install Extension

```bash
# Navigate to extension directory
cd /opt/postgresql/extensions/documentdb

# Set PostgreSQL configuration path
export PG_CONFIG=/usr/bin/pg_config

# Build the extension
sudo make clean
sudo make

# Install the extension
sudo make install

# Verify installation
ls -la /usr/lib64/pgsql/documentdb.so
```

### 4. Create Extension in Database

```sql
-- Connect to FauxDB database as postgres superuser
sudo -u postgres psql -d fauxdb

-- Create DocumentDB extension
CREATE EXTENSION IF NOT EXISTS documentdb;

-- Verify extension installation
SELECT extname, extversion FROM pg_extension WHERE extname = 'documentdb';
```

### 5. Test Extension Functionality

```sql
-- Create test table with JSONB column
CREATE TABLE test_docs (
    id SERIAL PRIMARY KEY,
    document JSONB
);

-- Insert test data
INSERT INTO test_docs (document) VALUES 
('{"name": "John", "age": 30, "tags": ["developer", "javascript"]}'),
('{"name": "Jane", "age": 25, "tags": ["designer", "css"]}'),
('{"name": "Bob", "age": 35, "tags": ["developer", "python"]}');

-- Test DocumentDB-style queries
SELECT * FROM test_docs WHERE documentdb_query(document, '{"age": {"$gte": 30}}');

-- Test array queries
SELECT * FROM test_docs WHERE documentdb_query(document, '{"tags": {"$in": ["developer"]}}');

-- Test aggregation-style operations
SELECT documentdb_aggregate(document, '[{"$group": {"_id": "$tags", "count": {"$sum": 1}}}]')
FROM test_docs;
```

## Configuration

### PostgreSQL Configuration

Update PostgreSQL configuration for optimal DocumentDB performance:

```bash
# Edit PostgreSQL configuration
sudo nano /var/lib/pgsql/data/postgresql.conf
```

Add these settings:

```ini
# Memory settings for DocumentDB operations
shared_buffers = 256MB
work_mem = 4MB
maintenance_work_mem = 64MB

# JSONB-specific settings
max_stack_depth = 2MB

# Query planning
random_page_cost = 1.1
effective_io_concurrency = 200
```

### Restart PostgreSQL

```bash
# Restart PostgreSQL to apply configuration changes
sudo systemctl restart postgresql

# Verify configuration
sudo -u postgres psql -c "SHOW shared_buffers;"
sudo -u postgres psql -c "SHOW work_mem;"
```

## Integration with FauxDB

### Update FauxDB Configuration

Ensure FauxDB is configured to use the DocumentDB extension:

```toml
# config/fauxdb.toml
[database]
uri = "postgresql://postgres@localhost:5432/fauxdb"
max_connections = 10
enable_documentdb_extensions = true
documentdb_extension = "documentdb"
```

### Test FauxDB Integration

```bash
# Start FauxDB server
cd /home/pgedge/pge/fauxdb
./target/release/fauxdb --config config/fauxdb.toml &

# Test MongoDB compatibility
mongosh mongodb://localhost:27018/test --eval "
    db.users.insertOne({name: 'Test User', age: 30});
    db.users.find({age: {\$gte: 25}});
    db.users.aggregate([{\$group: {_id: null, avgAge: {\$avg: '\$age'}}}]);
"
```

## Performance Optimization

### Create Optimized Indexes

```sql
-- Connect to FauxDB database
sudo -u postgres psql -d fauxdb

-- Create GIN index for JSONB columns
CREATE INDEX CONCURRENTLY idx_docs_gin 
ON collection_name USING GIN (document);

-- Create specific field indexes for common queries
CREATE INDEX CONCURRENTLY idx_docs_email 
ON collection_name USING GIN ((document->>'email'));

-- Create compound indexes
CREATE INDEX CONCURRENTLY idx_docs_age_status 
ON collection_name USING GIN ((document->'age'), (document->'status'));
```

### Monitor Performance

```sql
-- Check extension usage
SELECT 
    schemaname,
    tablename,
    attname,
    n_distinct,
    correlation
FROM pg_stats 
WHERE schemaname = 'public' 
AND tablename LIKE '%collection%';

-- Monitor query performance
SELECT 
    query,
    calls,
    total_time,
    mean_time,
    rows
FROM pg_stat_statements 
WHERE query LIKE '%documentdb%'
ORDER BY total_time DESC;
```

## Troubleshooting

### Common Issues

#### 1. Extension Build Errors

```bash
# Check PostgreSQL development headers
pg_config --version
pg_config --includedir

# Verify GCC version
gcc --version

# Clean and rebuild
cd /opt/postgresql/extensions/documentdb
sudo make clean
sudo make
```

#### 2. Extension Creation Errors

```sql
-- Check PostgreSQL superuser privileges
SELECT rolname, rolsuper FROM pg_roles WHERE rolname = 'postgres';

-- Check extension availability
SELECT * FROM pg_available_extensions WHERE name = 'documentdb';

-- Check installation path
\dx
```

#### 3. Performance Issues

```sql
-- Check index usage
EXPLAIN (ANALYZE, BUFFERS) 
SELECT * FROM collection_name 
WHERE documentdb_query(document, '{"field": "value"}');

-- Monitor query performance
SELECT 
    query,
    calls,
    total_time,
    mean_time,
    rows
FROM pg_stat_statements 
WHERE query LIKE '%documentdb%'
ORDER BY total_time DESC;
```

### Log Analysis

```bash
# Check PostgreSQL logs
sudo tail -f /var/lib/pgsql/data/log/postgresql-*.log

# Monitor FauxDB logs
tail -f /var/log/fauxdb.log

# Check system resources
top -p $(pgrep postgres)
iostat -x 1
```

## Security Considerations

### Extension Permissions

```sql
-- Grant necessary permissions
GRANT USAGE ON SCHEMA public TO fauxdb_user;
GRANT CREATE ON DATABASE fauxdb TO fauxdb_user;

-- Revoke unnecessary permissions
REVOKE CREATE ON SCHEMA public FROM public;
```

### Network Security

```bash
# Configure pg_hba.conf for secure connections
sudo nano /var/lib/pgsql/data/pg_hba.conf
```

```ini
# Local connections only
local   fauxdb          postgres                                peer
host    fauxdb          postgres        127.0.0.1/32            md5
host    fauxdb          postgres        ::1/128                 md5
```

## Maintenance

### Extension Updates

```bash
# Update DocumentDB extension
cd /opt/postgresql/extensions/documentdb
sudo git pull origin main
sudo make clean
sudo make
sudo make install

# Update extension in database
sudo -u postgres psql -d fauxdb -c "ALTER EXTENSION documentdb UPDATE;"
```

### Regular Maintenance

```sql
-- Analyze tables for query optimization
ANALYZE;

-- Update statistics
UPDATE pg_stat_user_tables SET n_tup_ins = 0, n_tup_upd = 0, n_tup_del = 0;

-- Reindex if necessary
REINDEX DATABASE fauxdb;
```

## Best Practices

### 1. Indexing Strategy

- Use GIN indexes for JSONB columns
- Create specific field indexes for frequently queried paths
- Monitor index usage and drop unused indexes
- Use partial indexes for conditional queries

### 2. Query Optimization

- Use DocumentDB functions for complex MongoDB-style queries
- Use native JSONB operators for simple queries
- Leverage PostgreSQL's query planner
- Monitor query performance regularly

### 3. Resource Management

- Configure appropriate memory settings
- Monitor connection pools
- Use connection pooling for high-concurrency applications
- Implement proper backup and recovery procedures

## Support and Resources

### Documentation References
- [PostgreSQL JSONB Documentation](https://www.postgresql.org/docs/current/datatype-json.html)
- [DocumentDB Compatibility Layer](https://github.com/aws/documentdb-compatibility-layer)
- [PostgreSQL Extension Development](https://www.postgresql.org/docs/current/extend.html)

### Community Support
- PostgreSQL Mailing Lists
- FauxDB GitHub Issues
- Stack Overflow PostgreSQL Tag

This guide ensures proper installation and configuration of the DocumentDB extension in PostgreSQL, providing enhanced MongoDB compatibility for FauxDB deployments.