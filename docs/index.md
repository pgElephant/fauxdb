---
layout: default
title: FauxDB - MongoDB Compatible Database
description: Production-ready MongoDB-compatible database server built in Rust with PostgreSQL backend
nav_order: 1
---

# FauxDB Documentation

Welcome to FauxDB, the production-ready MongoDB-compatible database server that translates MongoDB queries to PostgreSQL with native JSONB support.

## Quick Start

Get up and running with FauxDB in minutes:

```bash
# Clone and build
git clone https://github.com/fauxdb/fauxdb.git
cd fauxdb
./build.sh

# Start server
./run.sh

# Test compatibility
./test.sh
```

## Architecture

FauxDB provides full MongoDB wire protocol compatibility while leveraging PostgreSQL's robust JSONB capabilities:

<div class="mermaid">
graph LR
    A[MongoDB Client] -->|Wire Protocol| B[FauxDB Server]
    B -->|Query Translation| C[PostgreSQL + JSONB]
    B -->|Monitoring| D[Prometheus/Grafana]
    B -->|Logging| E[Structured Logs]
</div>

## Key Features

- **100% MongoDB Compatibility** - Use existing drivers and applications
- **High Performance** - Built in Rust for superior speed and memory efficiency
- **Production Ready** - Enterprise-grade monitoring, logging, and configuration
- **ACID Compliance** - Full PostgreSQL ACID transactions
- **Advanced Queries** - Rich aggregation pipelines and geospatial support
- **Real-time Monitoring** - Prometheus metrics and Grafana dashboards

## Documentation Sections

### Getting Started
- [Installation Guide]({{ site.baseurl }}/guides/installation/)
- [Quick Start Tutorial]({{ site.baseurl }}/tutorials/quick-start/)
- [Configuration Reference]({{ site.baseurl }}/guides/configuration/)

### Development
- [API Reference]({{ site.baseurl }}/api/operations/)
- [MongoDB Compatibility]({{ site.baseurl }}/guides/compatibility/)
- [DocumentDB Extensions]({{ site.baseurl }}/guides/documentdb-extension/)
- [Query Examples]({{ site.baseurl }}/examples/queries/)

### Deployment
- [Production Deployment]({{ site.baseurl }}/deployment/production/)
- [Docker Deployment]({{ site.baseurl }}/deployment/docker/)
- [Kubernetes Deployment]({{ site.baseurl }}/deployment/kubernetes/)

### Advanced Topics
- [Performance Tuning]({{ site.baseurl }}/guides/performance/)
- [Monitoring & Observability]({{ site.baseurl }}/guides/monitoring/)
- [Security Configuration]({{ site.baseurl }}/guides/security/)

## Quick Links

- [GitHub Repository](https://github.com/pgelephant/fauxdb)
- [Issue Tracker](https://github.com/pgelephant/fauxdb/issues)
- [Contributing Guide](https://github.com/pgelephant/fauxdb/blob/main/CONTRIBUTING.md)
- [License](https://github.com/pgelephant/fauxdb/blob/main/LICENSE)

## What is FauxDB?

FauxDB is a MongoDB-compatible database server that provides the MongoDB API while using PostgreSQL as the storage backend. This approach offers several advantages:

- **Familiar API**: Use existing MongoDB drivers and applications without modification
- **ACID Guarantees**: Benefit from PostgreSQL's robust transaction support
- **SQL Access**: Query your data directly with SQL when needed
- **Performance**: Leverage PostgreSQL's optimized JSONB with GIN indexes
- **Reliability**: Built on PostgreSQL's proven stability and features

## Use Cases

FauxDB is ideal for:

- **Migration from MongoDB**: Gradual migration with zero code changes
- **Hybrid Workloads**: Applications that need both NoSQL and SQL access
- **ACID Requirements**: Applications requiring strong consistency guarantees
- **Cost Optimization**: Reduce licensing costs while maintaining compatibility
- **Multi-database Strategies**: Use PostgreSQL for both relational and document data

## Why Choose FauxDB?

| Feature | FauxDB | MongoDB | Amazon DocumentDB |
|---------|--------|---------|-------------------|
| MongoDB API | Yes | Yes | Yes |
| ACID Transactions | Yes | No | No |
| SQL Access | Yes | No | No |
| Open Source | Yes | No | No |
| Self-Hosted | Yes | Yes | No |
| Cost | Free | Paid | Paid |

## Performance

FauxDB delivers exceptional performance through:

- **Rust Implementation**: Memory-safe and high-performance core
- **PostgreSQL JSONB**: Optimized binary JSON storage
- **GIN Indexes**: Fast lookups on JSON documents
- **Connection Pooling**: Efficient database connections
- **Query Optimization**: Intelligent query translation

## Technology Stack

- **Core**: Rust for maximum performance and safety
- **Storage**: PostgreSQL with JSONB extensions
- **Protocol**: MongoDB Wire Protocol implementation
- **Monitoring**: Prometheus metrics and Grafana dashboards
- **Deployment**: Docker, Kubernetes, and cloud-native

## Support

- **Documentation**: This site provides comprehensive guides
- **GitHub Issues**: Report bugs and request features
- **Community**: Join our discussions and get help
- **Enterprise**: Contact us for commercial support

---

For detailed setup instructions, refer to our [Installation Guide]({{ site.baseurl }}/guides/installation/) or [Quick Start Tutorial]({{ site.baseurl }}/tutorials/quick-start/).
