---
layout: default
title: Installation Guide
parent: Getting Started
nav_order: 1
description: Complete installation guide for FauxDB on various platforms
---

# Installation Guide

This guide covers installing FauxDB on different operating systems and deployment environments.

## System Requirements

### Minimum Requirements
- **CPU**: 2 cores, 2.0 GHz
- **RAM**: 2 GB
- **Storage**: 10 GB free space
- **OS**: Linux (x86_64, ARM64), macOS, Windows

### Recommended Requirements
- **CPU**: 4+ cores, 3.0 GHz
- **RAM**: 8+ GB
- **Storage**: 50+ GB SSD
- **Network**: Gigabit Ethernet

## Prerequisites

### Required Dependencies
- **Rust**: 1.70+ (automatically installed by build script)
- **PostgreSQL**: 13+ (automatically configured)
- **Git**: For cloning the repository

### Optional Dependencies
- **Docker/Podman**: For containerized deployment
- **Kubernetes**: For orchestrated deployment
- **Prometheus**: For monitoring
- **Grafana**: For dashboards

## Installation Methods

### Method 1: Automated Build Script (Recommended)

The recommended installation method uses our automated build script:

```bash
# Clone the repository
git clone https://github.com/pgelephant/fauxdb.git
cd fauxdb

# Run the build script (handles everything)
./build.sh

# Start FauxDB
./run.sh
```

The build script will:
- Install all system dependencies
- Set up PostgreSQL with proper configuration
- Install and configure Rust toolchain
- Build FauxDB with optimizations
- Create utility scripts
- Set up MongoDB test container

### Method 2: Manual Installation

For manual installation with custom configuration:

#### Step 1: Install System Dependencies

**Rocky Linux / RHEL / CentOS / Fedora:**
```bash
sudo dnf update -y
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y \
    git \
    curl \
    wget \
    gcc \
    gcc-c++ \
    make \
    cmake \
    pkg-config \
    openssl-devel \
    postgresql \
    postgresql-server \
    postgresql-contrib \
    postgresql-devel \
    python3-pymongo \
    mongo-c-driver \
    docker \
    podman \
    bison \
    flex
```

**Ubuntu / Debian:**
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    git \
    curl \
    wget \
    cmake \
    pkg-config \
    libssl-dev \
    postgresql \
    postgresql-contrib \
    postgresql-server-dev-all \
    python3-pymongo \
    libmongoc-dev \
    libbson-dev \
    docker.io \
    bison \
    flex
```

**macOS (with Homebrew):**
```bash
brew install git curl wget cmake pkg-config openssl postgresql bison flex
```

#### Step 2: Setup PostgreSQL

```bash
# Initialize PostgreSQL
sudo postgresql-setup --initdb  # Linux
# or
brew services start postgresql  # macOS

# Configure authentication (keep peer for postgres user)
sudo sed -i 's/host    all             all             127.0.0.1\/32            ident/host    all             all             127.0.0.1\/32            md5/' /var/lib/pgsql/data/pg_hba.conf

# Start PostgreSQL
sudo systemctl enable postgresql
sudo systemctl start postgresql

# Create database
sudo -u postgres psql -c "CREATE DATABASE fauxdb;"
sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE fauxdb TO postgres;"

# Install extensions
sudo -u postgres psql -d fauxdb -c "CREATE EXTENSION IF NOT EXISTS btree_gin;"
sudo -u postgres psql -d fauxdb -c "CREATE EXTENSION IF NOT EXISTS btree_gist;"
sudo -u postgres psql -d fauxdb -c "CREATE EXTENSION IF NOT EXISTS pg_trgm;"
sudo -u postgres psql -d fauxdb -c "CREATE EXTENSION IF NOT EXISTS plpython3u;"
```

#### Step 3: Install Rust

```bash
# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source ~/.cargo/env

# Update to latest stable
rustup update stable
rustup default stable

# Add components
rustup component add rustfmt clippy
```

#### Step 4: Build FauxDB

```bash
# Clone repository
git clone https://github.com/pgelephant/fauxdb.git
cd fauxdb

# Build in release mode
cargo build --release

# Verify build
ls -la target/release/fauxdb
```

### Method 3: Docker Installation

For containerized deployment:

```bash
# Clone repository
git clone https://github.com/pgelephant/fauxdb.git
cd fauxdb

# Build Docker image
docker build -t fauxdb:latest .

# Run with Docker Compose
docker-compose up -d
```

## Configuration

### Basic Configuration

Create or edit `config/fauxdb.toml`:

```toml
[server]
host = "127.0.0.1"
port = 27018
max_connections = 1000
connection_timeout_ms = 5000
idle_timeout_ms = 60000

[database]
uri = "postgresql://postgres@localhost:5432/fauxdb"
max_connections = 10
connection_timeout_ms = 5000
idle_timeout_ms = 60000
enable_jsonb_extensions = true

[logging]
level = "info"
format = "json"
output = "stdout"

[metrics]
enabled = true
port = 9090
path = "/metrics"

[ssl]
enabled = false

[authentication]
enabled = false
default_auth_method = "SCRAM-SHA-256"
session_timeout_minutes = 30
max_sessions_per_user = 10
```

### Advanced Configuration

For production deployments, see the [Configuration Reference]({{ site.baseurl }}/guides/configuration/) for detailed options.

## Verification

### Test Installation

```bash
# Start FauxDB
./target/release/fauxdb --config config/fauxdb.toml

# In another terminal, test connection
mongosh mongodb://localhost:27018/test --eval "db.runCommand({ping: 1})"
```

### Run Test Suite

```bash
# Run comprehensive tests
cargo test --test fauxdb_tests

# Test specific functionality
cargo test --test fauxdb_tests test_crud_operations
cargo test --test fauxdb_tests test_aggregation_pipeline
```

## Platform-Specific Notes

### Linux

- **Systemd Service**: Create a systemd service for automatic startup
- **Firewall**: Configure firewall rules for port 27018
- **SELinux**: May require additional configuration for PostgreSQL

### macOS

- **Homebrew**: Use Homebrew for package management
- **Ports**: PostgreSQL uses default port 5432
- **Permissions**: May need to adjust PostgreSQL permissions

### Windows

- **WSL**: Recommended to use Windows Subsystem for Linux
- **Native**: Requires Visual Studio Build Tools
- **Services**: Use Windows Services for PostgreSQL

## Troubleshooting

### Common Issues

#### PostgreSQL Connection Failed
```bash
# Check PostgreSQL status
sudo systemctl status postgresql

# Check authentication
sudo cat /var/lib/pgsql/data/pg_hba.conf

# Test connection
sudo -u postgres psql -d fauxdb
```

#### Build Errors
```bash
# Clean build cache
cargo clean

# Update Rust
rustup update

# Check dependencies
cargo check
```

#### Permission Issues
```bash
# Fix PostgreSQL permissions
sudo chown -R postgres:postgres /var/lib/pgsql/data
sudo chmod 700 /var/lib/pgsql/data
```

### Getting Help

- **Documentation**: Check other guides in this documentation
- **GitHub Issues**: Report bugs and request features
- **Community**: Join discussions on GitHub Discussions
- **Logs**: Check FauxDB logs for detailed error information

## Next Steps

After successful installation:

1. **Configure FauxDB**: See [Configuration Reference]({{ site.baseurl }}/guides/configuration/)
2. **Run Quick Start**: Try the [Quick Start Tutorial]({{ site.baseurl }}/tutorials/quick-start/)
3. **Test Compatibility**: Verify MongoDB compatibility
4. **Deploy to Production**: Follow [Production Deployment Guide]({{ site.baseurl }}/deployment/production/)

## Security Considerations

- **Authentication**: Enable authentication for production
- **SSL/TLS**: Configure SSL for encrypted connections
- **Network**: Use firewalls and VPNs for network security
- **Updates**: Keep system and dependencies updated
- **Backups**: Implement regular backup strategies
