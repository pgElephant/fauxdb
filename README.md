# FauxDB - Production-Ready MongoDB Alternative

[![Build Status](https://github.com/fauxdb/fauxdb/workflows/CI/badge.svg)](https://github.com/fauxdb/fauxdb/actions)
[![CircleCI](https://circleci.com/gh/fauxdb/fauxdb.svg?style=svg)](https://circleci.com/gh/fauxdb/fauxdb)
[![Code Coverage](https://codecov.io/gh/fauxdb/fauxdb/branch/main/graph/badge.svg)](https://codecov.io/gh/fauxdb/fauxdb)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Rust](https://img.shields.io/badge/rust-1.70%2B-orange.svg)](https://www.rust-lang.org)
[![MongoDB Compatible](https://img.shields.io/badge/MongoDB-5.0%2B-green.svg)](https://www.mongodb.com)
[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-17%2B-blue.svg)](https://www.postgresql.org)

**FauxDB is a high-performance, production-ready MongoDB-compatible database server built in Rust with full wire protocol compatibility.**

## Key Features

- ✓ **100% MongoDB Compatibility** - Full wire protocol support with `mongosh` compatibility
- ✓ **High Performance** - Built in Rust for superior speed and memory efficiency
- ✓ **Production Ready** - Enterprise-grade monitoring, logging, and configuration
- ✓ **Advanced Features** - Transactions, geospatial, aggregation pipelines
- ✓ **PostgreSQL Backend** - Leverages proven PostgreSQL reliability and features

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   MongoDB       │    │     FauxDB       │    │   PostgreSQL    │
│   Client        │◄──►│   (Rust Core)    │◄──►│  + DocumentDB   │
│                 │    │                  │    │   Extensions    │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                              │
                              ▼
                       ┌──────────────────┐
                       │   Advanced       │
                       │   Features:      │
                       │   • Transactions │
                       │   • Geospatial   │
                       │   • Aggregation  │
                       │   • Monitoring   │
                       └──────────────────┘
```

## Quick Start

### Prerequisites
- Rust 1.70+
- PostgreSQL 17+
- 2GB+ RAM recommended

### Installation

```bash
# Clone the repository
git clone https://github.com/fauxdb/fauxdb.git
cd fauxdb

# Build with optimizations
cargo build --release

# Run with default configuration
./target/release/fauxdb
```

### Docker Quick Start

```bash
# Start FauxDB with PostgreSQL
docker-compose up -d

# Connect with MongoDB client
mongosh mongodb://localhost:27018
```

## Testing

### Run All Tests

```bash
# Run comprehensive test suite using mongosh
cargo test --test fauxdb_tests

# Run specific test categories
cargo test --test fauxdb_tests test_crud_operations
cargo test --test fauxdb_tests test_aggregation_pipeline
```

### Test Coverage

```bash
# Generate code coverage report
cargo tarpaulin --out Html

# View coverage report
open tarpaulin-report.html
```

## MongoDB Compatibility

### ✓ **Supported Operations**

#### Core Operations
- `find`, `findOne`, `insertOne`, `insertMany`
- `updateOne`, `updateMany`, `deleteOne`, `deleteMany`
- `count`, `distinct`, `aggregate`
- `createIndex`, `dropIndex`, `listIndexes`

#### Advanced Operations
- **Transactions**: `startTransaction`, `commitTransaction`, `abortTransaction`
- **Geospatial**: `$geoNear`, `$geoWithin`, `$geoIntersects`
- **Aggregation**: 40+ pipeline stages with advanced operators
- **Change Streams**: Real-time data change notifications

#### Enterprise Features
- **Authentication**: SCRAM, X.509, LDAP integration
- **Authorization**: Role-based access control
- **Auditing**: Comprehensive audit logging
- **Monitoring**: Prometheus metrics, health checks

## Configuration

### Basic Configuration (`config/fauxdb.toml`)

```toml
[server]
host = "0.0.0.0"
port = 27018
max_connections = 10000
connection_timeout_ms = 30000

[database]
uri = "postgresql://localhost:5432/fauxdb"
max_connections = 100
enable_documentdb_extensions = true

[logging]
level = "info"
format = "json"

[metrics]
enabled = true
port = 9090
```

### Advanced Features

```toml
[transactions]
enabled = true
max_commit_time_ms = 5000
read_concern = "majority"
write_concern = "majority"

[geospatial]
enable_postgis = true
default_crs = "EPSG:4326"

[aggregation]
max_pipeline_depth = 100
enable_computed_fields = true
```

## Advanced Features

### 1. **Transaction Support**

```javascript
// Start a transaction
const session = client.startSession();
session.startTransaction();

try {
    // Perform operations
    await db.users.insertOne({name: "John"}, {session});
    await db.profiles.insertOne({userId: "123"}, {session});
    
    // Commit transaction
    await session.commitTransaction();
} catch (error) {
    // Abort on error
    await session.abortTransaction();
    throw error;
}
```

### 2. **Advanced Geospatial Queries**

```javascript
// Create 2dsphere index
await db.locations.createIndex({location: "2dsphere"});

// Find nearby locations
const nearby = await db.locations.find({
    location: {
        $near: {
            $geometry: {type: "Point", coordinates: [-74.0, 40.7]},
            $maxDistance: 1000
        }
    }
});
```

### 3. **Rich Aggregation Pipeline**

```javascript
// Complex aggregation with advanced stages
const result = await db.sales.aggregate([
    { $match: { date: { $gte: new Date("2024-01-01") } } },
    { $bucket: {
        groupBy: "$amount",
        boundaries: [0, 100, 500, 1000, Infinity],
        default: "Other"
    }},
    { $facet: {
        "totalSales": [{ $group: { _id: null, total: { $sum: "$amount" } } }],
        "avgSales": [{ $group: { _id: null, avg: { $avg: "$amount" } } }]
    }}
]);
```

## Monitoring & Observability

### Prometheus Metrics

```bash
# Access metrics endpoint
curl http://localhost:9090/metrics

# Key metrics:
# - fauxdb_operations_total
# - fauxdb_operation_duration_seconds
# - fauxdb_connections_active
# - fauxdb_transactions_total
```

### Health Checks

```bash
# Basic health check
curl http://localhost:9090/health

# Detailed status
curl http://localhost:9090/status
```

## Deployment

### Production Deployment

```yaml
# docker-compose.yml
version: '3.8'
services:
  fauxdb:
    image: fauxdb:latest
    ports:
      - "27018:27018"
    environment:
      - DATABASE_URL=postgresql://postgres:password@postgres:5432/fauxdb
    depends_on:
      - postgres
  
  postgres:
    image: postgres:17
    environment:
      POSTGRES_DB: fauxdb
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: password
    volumes:
      - postgres_data:/var/lib/postgresql/data

volumes:
  postgres_data:
```

### Kubernetes Deployment

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: fauxdb
spec:
  replicas: 3
  selector:
    matchLabels:
      app: fauxdb
  template:
    metadata:
      labels:
        app: fauxdb
    spec:
      containers:
      - name: fauxdb
        image: fauxdb:latest
        ports:
        - containerPort: 27018
        env:
        - name: DATABASE_URL
          value: "postgresql://postgres:password@postgres:5432/fauxdb"
```

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

### Development Setup

```bash
# Clone and setup
git clone https://github.com/fauxdb/fauxdb.git
cd fauxdb

# Install dependencies
cargo build

# Run tests
cargo test --test fauxdb_tests

# Format code
cargo fmt

# Run linter
cargo clippy

# Check coverage
cargo tarpaulin --out Html
```

## License

FauxDB is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Built in Rust
- Powered by PostgreSQL and DocumentDB extensions
- Inspired by MongoDB's excellent API design

---

**FauxDB: The MongoDB alternative that doesn't compromise on performance, features, or reliability.**