# FauxDB Docker Configuration

This directory contains all Docker-related configuration files for FauxDB.

## Directory Structure

```
docker/
├── README.md                    # This file
├── DOCKER.md                    # Detailed Docker documentation
├── Dockerfile                   # Production Docker image
├── Dockerfile.dev               # Development Docker image
├── .dockerignore                # Docker ignore file
├── compose/                     # Docker Compose files
│   ├── docker-compose.yml       # Basic setup
│   ├── docker-compose.dev.yml   # Development environment
│   └── docker-compose.prod.yml  # Production environment
├── config/                      # Configuration files
│   └── docker.env.example       # Environment variables template
├── postgres/                    # PostgreSQL configuration
│   └── init.sql                 # Database initialization script
└── scripts/                     # Docker utility scripts (future)
```

## Quick Start

### Development
```bash
# From project root
make dev
```

### Production
```bash
# Copy environment file
cp docker/config/docker.env.example .env
# Edit .env with your settings
make prod
```

### Basic Setup
```bash
# From project root
make up
```

## Docker Compose Files

### `compose/docker-compose.yml`
- Basic FauxDB setup with PostgreSQL
- MongoDB shell for testing
- Suitable for development and testing

### `compose/docker-compose.dev.yml`
- Development environment with hot reload
- Debug logging enabled
- PgAdmin for database management
- Volume mounting for live code changes

### `compose/docker-compose.prod.yml`
- Production-optimized setup
- Resource limits and reservations
- Optional monitoring stack
- SSL and authentication ready

## Docker Images

### `Dockerfile` (Production)
- Multi-stage build for optimized size
- Based on Debian Bookworm Slim
- Non-root user for security
- Health checks included

### `Dockerfile.dev` (Development)
- Full Rust development environment
- MongoDB shell included
- Development tools pre-installed

## Configuration

### Environment Variables
Copy `config/docker.env.example` to `.env` in the project root and configure:

```bash
# PostgreSQL
POSTGRES_USER=fauxdb
POSTGRES_PASSWORD=your_secure_password
POSTGRES_DB=fauxdb_prod

# FauxDB Server
FAUXDB_PORT=27018
FAUXDB_MAX_CONNECTIONS=1000

# Security
FAUXDB_ENABLE_SSL=false
FAUXDB_ENABLE_AUTH=false
```

### PostgreSQL Initialization
The `postgres/init.sql` script:
- Creates required extensions
- Sets up FauxDB schemas
- Inserts sample data
- Configures permissions

## Services

### Core Services
- **fauxdb** - FauxDB server (port 27018/27019)
- **postgres** - PostgreSQL database (port 5432/5433)

### Optional Services
- **mongosh** - MongoDB shell for testing
- **pgadmin** - PostgreSQL admin interface (port 5050)
- **prometheus** - Metrics collection (port 9090)
- **grafana** - Monitoring dashboards (port 3000)

## Volumes

- `postgres_data` - PostgreSQL data persistence
- `fauxdb_logs` - FauxDB application logs
- `prometheus_data` - Prometheus metrics storage
- `grafana_data` - Grafana dashboards and settings

## Networks

- `fauxdb-network` - Basic setup
- `fauxdb-dev-network` - Development environment
- `fauxdb-prod-network` - Production environment

## Make Commands

From the project root, use these commands:

```bash
# Development
make dev          # Start development environment
make dev-logs     # View development logs
make dev-shell    # Open shell in dev container

# Production
make prod         # Start production environment
make prod-logs    # View production logs
make monitor      # Start with monitoring

# Testing
make test         # Run tests with Docker
make test-mongosh # Test with mongosh client
make perf-test    # Run performance tests

# Database
make db-shell     # Open PostgreSQL shell
make db-backup    # Backup database
make db-restore   # Restore database

# Utilities
make clean        # Clean up Docker resources
make status       # Show service status
make health       # Check service health
```

## Troubleshooting

### Common Issues

1. **Port conflicts**
   ```bash
   lsof -i :27018
   lsof -i :5432
   ```

2. **Permission issues**
   ```bash
   sudo chown -R $(id -u):$(id -g) ./data
   ```

3. **Container won't start**
   ```bash
   make dev-logs
   ```

### Debug Mode
```bash
export RUST_LOG=debug
export RUST_BACKTRACE=1
make dev
```

## Security

### Production Security Features
- Non-root user in containers
- Read-only configuration mounts
- Resource limits and reservations
- Health checks for all services
- Restart policies for high availability

### SSL/TLS Support
```bash
FAUXDB_ENABLE_SSL=true
FAUXDB_SSL_CERT_PATH=/app/certs/server.crt
FAUXDB_SSL_KEY_PATH=/app/certs/server.key
```

## Monitoring

### Prometheus Metrics
- Available on port 9090
- FauxDB metrics endpoint: `/metrics`

### Grafana Dashboards
- Available on port 3000
- Pre-configured FauxDB dashboards

## Contributing

When contributing to Docker support:
1. Test with all Docker Compose files
2. Ensure health checks work
3. Update documentation
4. Test on different platforms

---

**FauxDB Docker Configuration** - Copyright (c) 2025 pgElephant. All rights reserved.
