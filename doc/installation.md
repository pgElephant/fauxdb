# Installation Guide

## Prerequisites

### Required Dependencies

- **libmongoc** (MongoDB C Driver) 2.0+
- **PostgreSQL** 12+ with development headers
- **nlohmann/json** library
- **CMake** 3.16+
- **C++20** compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)

### Platform-Specific Installation

#### macOS with Homebrew
```bash
brew install mongo-c-driver postgresql nlohmann-json cmake
```

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libpq-dev libmongoc-1.0-dev \
                     nlohmann-json3-dev libjson-c-dev
```

#### CentOS/RHEL/Fedora
```bash
# RHEL/CentOS 8+
sudo dnf install gcc-c++ cmake postgresql-devel mongo-c-driver-devel \
                 json-c-devel nlohmann-json-devel

# Older versions
sudo yum install gcc-c++ cmake postgresql-devel mongo-c-driver-devel \
                 json-c-devel
```

#### Alpine Linux
```bash
sudo apk add build-base cmake postgresql-dev mongo-c-driver-dev \
            nlohmann-json json-c-dev
```

## Building from Source

### Basic Build
```bash
# Clone the repository
git clone https://github.com/yourusername/fauxdb.git
cd fauxdb

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)
```

### Build Options

#### Debug Build
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

#### Release Build with Optimizations
```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native" ..
make -j$(nproc)
```

#### Custom Installation Prefix
```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/fauxdb ..
make -j$(nproc)
sudo make install
```

### Build with Tests
```bash
cmake -DBUILD_TESTS=ON ..
make -j$(nproc)
make test
```

## Installation

### System Installation
```bash
# From build directory
sudo make install

# Verify installation
fauxdb --version
```

### Manual Installation
```bash
# Copy binary
sudo cp fauxdb /usr/local/bin/

# Create configuration directory
sudo mkdir -p /etc/fauxdb

# Copy sample configuration
sudo cp ../config/fauxdb.conf /etc/fauxdb/

# Create log directory
sudo mkdir -p /var/log/fauxdb
sudo chown fauxdb:fauxdb /var/log/fauxdb  # if user exists
```

## System Service Setup

### Create Service User
```bash
# Create dedicated user for FauxDB
sudo useradd -r -s /bin/false -d /var/lib/fauxdb fauxdb

# Create required directories
sudo mkdir -p /var/lib/fauxdb /var/log/fauxdb /var/run/fauxdb
sudo chown fauxdb:fauxdb /var/lib/fauxdb /var/log/fauxdb /var/run/fauxdb
```

### SystemD Service
```bash
# Install service file
sudo cp config/fauxdb.service /etc/systemd/system/

# Reload systemd
sudo systemctl daemon-reload

# Enable service
sudo systemctl enable fauxdb

# Start service
sudo systemctl start fauxdb

# Check status
sudo systemctl status fauxdb
```

### SysV Init (older systems)
```bash
# Copy init script
sudo cp config/fauxdb.init /etc/init.d/fauxdb
sudo chmod +x /etc/init.d/fauxdb

# Enable service
sudo chkconfig fauxdb on

# Start service
sudo service fauxdb start
```

## Docker Installation

### Using Docker Compose
```yaml
# docker-compose.yml
version: '3.8'
services:
  postgres:
    image: postgres:14
    environment:
      POSTGRES_DB: fauxdb
      POSTGRES_USER: fauxdb
      POSTGRES_PASSWORD: password
    volumes:
      - postgres_data:/var/lib/postgresql/data
    ports:
      - "5432:5432"

  fauxdb:
    build: .
    ports:
      - "27018:27018"
    depends_on:
      - postgres
    environment:
      FAUXDB_PG_HOST: postgres
      FAUXDB_PG_DATABASE: fauxdb
      FAUXDB_PG_USER: fauxdb
      FAUXDB_PG_PASSWORD: password
    volumes:
      - ./config:/etc/fauxdb:ro
      - fauxdb_logs:/var/log/fauxdb

volumes:
  postgres_data:
  fauxdb_logs:
```

### Build and Run
```bash
docker-compose up -d
```

## Verification

### Test Installation
```bash
# Check if FauxDB is running
ps aux | grep fauxdb

# Test connection
mongosh --host localhost --port 27018 --eval "db.adminCommand('ping')"

# Check logs
tail -f /var/log/fauxdb/fauxdb.log
```

### Performance Test
```bash
# Simple benchmark
time mongosh --host localhost --port 27018 --eval "
for(let i=0; i<1000; i++) {
  db.test.insertOne({_id: i, value: 'test' + i});
}
db.test.find().count();
"
```

## Troubleshooting

### Common Build Issues

#### Missing Dependencies
```bash
# Ubuntu: Package not found
sudo apt-get update
sudo apt-get install pkg-config

# macOS: Homebrew issues
brew update && brew upgrade
```

#### CMake Version Too Old
```bash
# Install newer CMake
pip3 install cmake
# or
snap install cmake --classic
```

#### Compiler Issues
```bash
# Install newer GCC
sudo apt-get install gcc-10 g++-10
export CC=gcc-10 CXX=g++-10
```

### Runtime Issues

#### Permission Denied
```bash
# Fix permissions
sudo chown -R fauxdb:fauxdb /var/log/fauxdb /var/run/fauxdb
sudo chmod 755 /var/log/fauxdb /var/run/fauxdb
```

#### Port Already in Use
```bash
# Find process using port
sudo lsof -i :27018
sudo netstat -tlnp | grep 27018

# Kill conflicting process
sudo kill -9 <PID>
```

#### Database Connection Failed
```bash
# Test PostgreSQL connection
psql -h localhost -U fauxdb -d fauxdb

# Check PostgreSQL is running
sudo systemctl status postgresql
```

## Security Considerations

### File Permissions
```bash
# Secure configuration files
sudo chmod 600 /etc/fauxdb/fauxdb.conf
sudo chown root:fauxdb /etc/fauxdb/fauxdb.conf

# Secure log files
sudo chmod 640 /var/log/fauxdb/*.log
sudo chown fauxdb:adm /var/log/fauxdb/*.log
```

### Firewall Configuration
```bash
# Allow FauxDB port
sudo ufw allow 27018/tcp

# Or with iptables
sudo iptables -A INPUT -p tcp --dport 27018 -j ACCEPT
```

### PostgreSQL Security
```bash
# Create dedicated database user
sudo -u postgres createuser fauxdb
sudo -u postgres createdb -O fauxdb fauxdb

# Set strong password
sudo -u postgres psql -c "ALTER USER fauxdb PASSWORD 'strong_password';"
```

## Next Steps

After successful installation:

1. [Configure FauxDB](configuration.md) - Set up your server configuration
2. [Usage Guide](usage.md) - Learn how to use FauxDB
3. [Architecture Overview](architecture.md) - Understand how FauxDB works
4. [Development Setup](development.md) - Set up development environment
