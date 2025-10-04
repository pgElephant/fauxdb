#!/bin/bash

# FauxDB Build Script
# This script handles all dependencies and builds the complete system
# Run with: ./build.sh

set -e  # Exit on any error

echo "FauxDB Build Script Starting..."
echo "==============================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
if [[ $EUID -eq 0 ]]; then
   print_error "This script should not be run as root"
   exit 1
fi

# Detect OS
if [[ -f /etc/os-release ]]; then
    . /etc/os-release
    OS=$ID
    OS_VERSION=$VERSION_ID
else
    print_error "Cannot detect OS"
    exit 1
fi

print_status "Detected OS: $OS $OS_VERSION"

# Function to install system dependencies
install_system_deps() {
    print_status "Installing system dependencies..."
    
    if [[ "$OS" == "rocky" ]] || [[ "$OS" == "rhel" ]] || [[ "$OS" == "centos" ]] || [[ "$OS" == "fedora" ]]; then
        # Rocky Linux / RHEL / CentOS / Fedora
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
        
        # Enable PostgreSQL
        sudo systemctl enable postgresql
        sudo systemctl start postgresql
        
    elif [[ "$OS" == "ubuntu" ]] || [[ "$OS" == "debian" ]]; then
        # Ubuntu / Debian
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
        
        # Enable PostgreSQL
        sudo systemctl enable postgresql
        sudo systemctl start postgresql
        
    else
        print_error "Unsupported OS: $OS"
        exit 1
    fi
    
    print_success "System dependencies installed"
}

# Function to setup PostgreSQL
setup_postgresql() {
    print_status "Setting up PostgreSQL..."
    
    # Initialize database if not already done
    if sudo test -f /var/lib/pgsql/data/postgresql.conf; then
        print_status "PostgreSQL already initialized, skipping..."
    else
        print_status "Initializing PostgreSQL database..."
        sudo postgresql-setup --initdb
    fi
    
    # Configure authentication (keep peer for postgres user)
    sudo sed -i 's/host    all             all             127.0.0.1\/32            ident/host    all             all             127.0.0.1\/32            md5/' /var/lib/pgsql/data/pg_hba.conf
    sudo sed -i 's/host    all             all             ::1\/128                 ident/host    all             all             ::1\/128                 md5/' /var/lib/pgsql/data/pg_hba.conf
    
    # Restart PostgreSQL
    sudo systemctl restart postgresql
    
    # Create database
    sudo -u postgres psql -c "CREATE DATABASE fauxdb;" || true
    sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE fauxdb TO postgres;" || true
    
    # Install standard PostgreSQL extensions (included with PostgreSQL)
    sudo -u postgres psql -d fauxdb -c "CREATE EXTENSION IF NOT EXISTS btree_gin;" || true
    sudo -u postgres psql -d fauxdb -c "CREATE EXTENSION IF NOT EXISTS btree_gist;" || true
    sudo -u postgres psql -d fauxdb -c "CREATE EXTENSION IF NOT EXISTS pg_trgm;" || true
    sudo -u postgres psql -d fauxdb -c "CREATE EXTENSION IF NOT EXISTS plpython3u;" || true
    
    print_status "Standard PostgreSQL extensions installed"
    print_status "Note: DocumentDB third-party extension must be installed separately"
    print_status "See: docs/guides/documentdb-extension.md"
    
    print_success "PostgreSQL setup complete"
}

# Function to install Rust toolchain
install_rust() {
    print_status "Installing Rust toolchain..."
    
    # Check if rustup is already installed
    if ! command -v rustup &> /dev/null; then
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
        source ~/.cargo/env
    fi
    
    # Update to latest stable
    rustup update stable
    rustup default stable
    
    # Add components
    rustup component add rustfmt clippy
    
    print_success "Rust toolchain installed: $(rustc --version)"
}

# Function to build FauxDB
build_fauxdb() {
    print_status "Building FauxDB..."
    
    cd /home/pgedge/pge/fauxdb
    
    # Clean previous builds
    cargo clean
    
    # Build in release mode
    cargo build --release
    
    print_success "FauxDB built successfully"
}

# Function to setup MongoDB for testing
setup_mongodb() {
    print_status "Setting up MongoDB for testing..."
    
    # Start MongoDB container
    podman run -d --name mongodb-test -p 27017:27017 docker.io/mongo:latest || true
    
    # Wait for MongoDB to be ready
    sleep 5
    
    # Test MongoDB connection
    if podman exec mongodb-test mongosh --eval "db.adminCommand('ping')" > /dev/null 2>&1; then
        print_success "MongoDB test container ready"
    else
        print_warning "MongoDB test container not ready, but continuing..."
    fi
}

# Function to test the build
test_build() {
    print_status "Testing the build..."
    
    cd /home/pgedge/pge/fauxdb
    
    # Test PostgreSQL connection
    if sudo -u postgres psql -d fauxdb -c "SELECT 1;" > /dev/null 2>&1; then
        print_success "PostgreSQL connection test passed"
    else
        print_error "PostgreSQL connection test failed"
        return 1
    fi
    
    # Test FauxDB binary
    if [[ -f target/release/fauxdb ]]; then
        print_success "FauxDB binary exists"
    else
        print_error "FauxDB binary not found"
        return 1
    fi
    
    print_success "All tests passed"
}

# Function to create run script
create_run_script() {
    print_status "Creating run script..."
    
    cat > /home/pgedge/pge/fauxdb/run.sh << 'EOF'
#!/bin/bash

# FauxDB Run Script
# Starts the FauxDB server with proper configuration

echo "Starting FauxDB Server..."
echo "========================="

# Check if PostgreSQL is running
if ! systemctl is-active --quiet postgresql; then
    echo "Starting PostgreSQL..."
    sudo systemctl start postgresql
fi

# Check if MongoDB test container is running
if ! podman ps | grep -q mongodb-test; then
    echo "Starting MongoDB test container..."
    podman run -d --name mongodb-test -p 27017:27017 docker.io/mongo:latest
    sleep 3
fi

# Start FauxDB
echo "Starting FauxDB server on port 27018..."
cd /home/pgedge/pge/fauxdb
RUSTUP_TOOLCHAIN=stable /home/pgedge/.rustup/toolchains/stable-aarch64-unknown-linux-gnu/bin/cargo run --release -- --config config/fauxdb.toml
EOF

    chmod +x /home/pgedge/pge/fauxdb/run.sh
    print_success "Run script created: ./run.sh"
}

# Function to create test script
create_test_script() {
    print_status "Creating test script..."
    
    cat > /home/pgedge/pge/fauxdb/test.sh << 'EOF'
#!/bin/bash

# FauxDB Test Script
# Tests MongoDB compatibility through FauxDB

echo "Testing FauxDB MongoDB Compatibility..."
echo "======================================"

# Test MongoDB shell connection to FauxDB
echo "Testing mongosh connection to FauxDB (port 27018)..."
podman exec mongodb-test mongosh --host localhost --port 27018 --eval "
    use test;
    db.users.insertOne({name: 'John', age: 30, email: 'john@example.com'});
    db.users.insertOne({name: 'Jane', age: 25, email: 'jane@example.com'});
    db.users.find().pretty();
    db.users.find({age: {\$gte: 30}}).pretty();
    db.users.countDocuments();
"

echo ""
echo "Testing aggregation pipeline..."
podman exec mongodb-test mongosh --host localhost --port 27018 --eval "
    use test;
    db.users.aggregate([
        {\$match: {age: {\$gte: 25}}},
        {\$group: {_id: null, avgAge: {\$avg: '\$age'}, count: {\$sum: 1}}}
    ]).pretty();
"

echo ""
echo "Test completed! Check the results above."
EOF

    chmod +x /home/pgedge/pge/fauxdb/test.sh
    print_success "Test script created: ./test.sh"
}

# Main build process
main() {
    echo ""
    print_status "Starting FauxDB build process..."
    echo ""
    
    # Install system dependencies
    install_system_deps
    
    # Setup PostgreSQL
    setup_postgresql
    
    # Install Rust toolchain
    install_rust
    
    # Build FauxDB
    build_fauxdb
    
    # Setup MongoDB for testing
    setup_mongodb
    
    # Test the build
    test_build
    
    # Create utility scripts
    create_run_script
    create_test_script
    
    echo ""
    print_success "FauxDB build completed successfully!"
    echo ""
    echo "Next steps:"
    echo "1. Start FauxDB server: ./run.sh"
    echo "2. Test MongoDB compatibility: ./test.sh"
    echo "3. Connect with mongosh: mongosh mongodb://localhost:27018"
    echo ""
    echo "Configuration:"
    echo "- FauxDB Server: localhost:27018"
    echo "- PostgreSQL: localhost:5432 (user: postgres, db: fauxdb)"
    echo "- MongoDB Test: localhost:27017 (for comparison)"
    echo ""
}

# Run main function
main "$@"
