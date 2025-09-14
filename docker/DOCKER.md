# FauxDB Docker Support

## Overview

FauxDB includes comprehensive Docker support for development, testing, and production deployments. This document provides detailed information about using FauxDB with Docker.

## Quick Start

```bash
# Clone the repository
git clone https://github.com/fauxdb/fauxdb.git
cd fauxdb

# Quick setup
make setup

# Start development environment
make dev
```

## Docker Compose Files

### 1. `docker-compose.yml` - Basic Setup
- FauxDB server with PostgreSQL backend
- MongoDB shell for testing
- Basic health checks
- Suitable for development and testing

### 2. `docker-compose.dev.yml` - Development Environment
- Hot reload with `cargo-watch`
- Debug logging enabled
- PostgreSQL on port 5433
- FauxDB on port 27019
- PgAdmin for database management
- Volume mounting for live code changes

### 3. `docker-compose.prod.yml` - Production Environment
- Optimized for production use
- Resource limits and reservations
- Optional monitoring stack (Prometheus + Grafana)
- SSL and authentication ready
- Health checks and restart policies

## Docker Images

### Production Image (`Dockerfile`)
- Multi-stage build for optimized size
- Based on Debian Bookworm Slim
- Non-root user for security
- Health checks included
- Minimal runtime dependencies

### Development Image (`Dockerfile.dev`)
- Full Rust development environment
- MongoDB shell included
- Development tools pre-installed
- Suitable for debugging and testing

## Environment Configuration

### Environment Variables

Copy `docker.env.example` to `.env` and configure:

```bash
# PostgreSQL Configuration
POSTGRES_USER=fauxdb
POSTGRES_PASSWORD=your_secure_password
POSTGRES_DB=fauxdb_prod

# FauxDB Server Configuration
FAUXDB_HOST=0.0.0.0
FAUXDB_PORT=27018
FAUXDB_MAX_CONNECTIONS=1000
FAUXDB_WORKER_THREADS=4

# Logging
RUST_LOG=info
RUST_BACKTRACE=0

# Security
FAUXDB_ENABLE_SSL=false
FAUXDB_ENABLE_AUTH=false

# Monitoring
GRAFANA_PASSWORD=admin123
```

## Available Commands

### Development Commands
```bash
make dev          # Start development environment
make dev-logs     # View development logs
make dev-shell    # Open shell in development container
make dev-stop     # Stop development environment
```

### Production Commands
```bash
make prod         # Start production environment
make prod-logs    # View production logs
make prod-shell   # Open shell in production container
make prod-stop    # Stop production environment
```

### Testing Commands
```bash
make test         # Run tests with Docker
make test-mongosh # Test with mongosh client
make perf-test    # Run performance tests
make health       # Check service health
```

### Database Commands
```bash
make db-shell     # Open PostgreSQL shell
make db-backup    # Backup database
make db-restore   # Restore database
```

### Monitoring Commands
```bash
make monitor      # Start monitoring stack
make monitor-stop # Stop monitoring services
```

### Utility Commands
```bash
make clean        # Clean up Docker resources
make clean-all    # Clean up all Docker resources
make status       # Show service status
make help         # Show all available commands
```

## Services

### Core Services

1. **fauxdb** - FauxDB server
   - Port: 27018 (production), 27019 (development)
   - Health check: MongoDB ping command
   - Restart policy: unless-stopped

2. **postgres** - PostgreSQL database
   - Port: 5432 (production), 5433 (development)
   - Health check: pg_isready
   - Data persistence with named volumes

### Optional Services

3. **mongosh** - MongoDB shell for testing
   - Runs tests against FauxDB
   - Available in testing profile

4. **pgadmin** - PostgreSQL admin interface
   - Port: 5050
   - Available in admin profile

5. **prometheus** - Metrics collection
   - Port: 9090
   - Available in monitoring profile

6. **grafana** - Monitoring dashboards
   - Port: 3000
   - Available in monitoring profile

## Volumes

### Named Volumes
- `postgres_data` - PostgreSQL data persistence
- `fauxdb_logs` - FauxDB application logs
- `prometheus_data` - Prometheus metrics storage
- `grafana_data` - Grafana dashboards and settings
- `cargo_cache` - Rust compilation cache (development)

### Bind Mounts
- `./config:/app/config:ro` - Configuration files
- `.:/app` - Source code (development only)

## Networks

- `fauxdb-network` - Basic setup
- `fauxdb-dev-network` - Development environment
- `fauxdb-prod-network` - Production environment

Each network uses a different subnet to avoid conflicts.

## Security

### Production Security Features
- Non-root user in containers
- Read-only configuration mounts
- Resource limits and reservations
- Health checks for all services
- Restart policies for high availability

### SSL/TLS Support
```bash
# Enable SSL in .env
FAUXDB_ENABLE_SSL=true
FAUXDB_SSL_CERT_PATH=/app/certs/server.crt
FAUXDB_SSL_KEY_PATH=/app/certs/server.key
```

### Authentication
```bash
# Enable authentication in .env
FAUXDB_ENABLE_AUTH=true
FAUXDB_AUTH_METHOD=SCRAM-SHA-256
```

## Monitoring

### Prometheus Metrics
- Available on port 9090
- FauxDB metrics endpoint: `/metrics`
- Custom dashboards available

### Grafana Dashboards
- Available on port 3000
- Pre-configured FauxDB dashboards
- PostgreSQL monitoring included

### Health Checks
- FauxDB: MongoDB ping command
- PostgreSQL: pg_isready
- All services have configurable health checks

## Troubleshooting

### Common Issues

1. **Port conflicts**
   ```bash
   # Check what's using the ports
   lsof -i :27018
   lsof -i :5432
   ```

2. **Permission issues**
   ```bash
   # Fix volume permissions
   sudo chown -R $(id -u):$(id -g) ./data
   ```

3. **Container won't start**
   ```bash
   # Check logs
   make dev-logs
   docker-compose logs fauxdb
   ```

4. **Database connection issues**
   ```bash
   # Test PostgreSQL connection
   make db-shell
   ```

### Debug Mode
```bash
# Enable debug logging
export RUST_LOG=debug
export RUST_BACKTRACE=1
make dev
```

## Performance Tuning

### Production Optimizations
- Resource limits configured
- Connection pooling enabled
- Health checks optimized
- Restart policies configured

### Development Optimizations
- Volume mounting for live reload
- Debug logging enabled
- Development tools included

## Backup and Recovery

### Database Backup
```bash
# Create backup
make db-backup

# Restore backup
make db-restore FILE=backup_20250101_120000.sql
```

### Volume Backup
```bash
# Backup volumes
docker run --rm -v fauxdb_postgres_data:/data -v $(pwd):/backup alpine tar czf /backup/postgres_backup.tar.gz -C /data .
```

## Contributing

When contributing to FauxDB Docker support:

1. Test with all Docker Compose files
2. Ensure health checks work
3. Update documentation
4. Test on different platforms

## Support

For Docker-related issues:
1. Check the logs: `make dev-logs`
2. Verify environment variables
3. Check Docker and Docker Compose versions
4. Review this documentation

---

**FauxDB Docker Support** - Copyright (c) 2025 pgElephant. All rights reserved.
