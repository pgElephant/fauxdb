#!/bin/bash

# FauxDB Build Script
# Builds the modern C++ FauxDB project with automatic dependency management

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
BUILD_TESTS="OFF"
BUILD_DIR="build"
INSTALL_DIR=""
CLEAN_BUILD=false
VERBOSE=false
AUTO_INSTALL_DEPS=true

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

# Function to detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if command -v apt-get &> /dev/null; then
            echo "ubuntu"
        elif command -v yum &> /dev/null; then
            echo "centos"
        elif command -v dnf &> /dev/null; then
            echo "fedora"
        elif command -v pacman &> /dev/null; then
            echo "arch"
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Function to install dependencies on Ubuntu/Debian
install_deps_ubuntu() {
    print_status "Installing dependencies on Ubuntu/Debian..."
    
    # Update package list
    sudo apt-get update
    
    # Install essential build tools
    sudo apt-get install -y build-essential cmake pkg-config
    
    # Install C++ compiler
    sudo apt-get install -y g++-13 || sudo apt-get install -y g++-12 || sudo apt-get install -y g++
    
    # Install required libraries
    sudo apt-get install -y libboost-all-dev
    sudo apt-get install -y libmongoc-dev
    sudo apt-get install -y libpq-dev
    sudo apt-get install -y libssl-dev
    sudo apt-get install -y libfmt-dev
    sudo apt-get install -y libspdlog-dev
    sudo apt-get install -y nlohmann-json3-dev
    sudo apt-get install -y libgtest-dev
    
    print_success "Dependencies installed on Ubuntu/Debian"
}

# Function to install dependencies on CentOS/RHEL
install_deps_centos() {
    print_status "Installing dependencies on CentOS/RHEL..."
    
    # Install EPEL repository
    sudo yum install -y epel-release
    
    # Install essential build tools
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y cmake3 pkg-config
    
    # Install required libraries
    sudo yum install -y boost-devel
    sudo yum install -y mongo-c-driver-devel
    sudo yum install -y postgresql-devel
    sudo yum install -y openssl-devel
    sudo yum install -y fmt-devel
    sudo yum install -y spdlog-devel
    sudo yum install -y nlohmann-json-devel
    sudo yum install -y gtest-devel
    
    print_success "Dependencies installed on CentOS/RHEL"
}

# Function to install dependencies on Fedora
install_deps_fedora() {
    print_status "Installing dependencies on Fedora..."
    
    # Install essential build tools
    sudo dnf groupinstall -y "Development Tools"
    sudo dnf install -y cmake pkg-config
    
    # Install required libraries
    sudo dnf install -y boost-devel
    sudo dnf install -y mongo-c-driver-devel
    sudo dnf install -y postgresql-devel
    sudo dnf install -y openssl-devel
    sudo dnf install -y fmt-devel
    sudo dnf install -y spdlog-devel
    sudo dnf install -y nlohmann-json-devel
    sudo dnf install -y gtest-devel
    
    print_success "Dependencies installed on Fedora"
}

# Function to install dependencies on Arch Linux
install_deps_arch() {
    print_status "Installing dependencies on Arch Linux..."
    
    # Install essential build tools
    sudo pacman -S --needed base-devel cmake pkg-config
    
    # Install required libraries
    sudo pacman -S --needed boost
    sudo pacman -S --needed mongo-c-driver
    sudo pacman -S --needed postgresql
    sudo pacman -S --needed openssl
    sudo pacman -S --needed fmt
    sudo pacman -S --needed spdlog
    sudo pacman -S --needed nlohmann-json
    sudo pacman -S --needed gtest
    
    print_success "Dependencies installed on Arch Linux"
}

# Function to install dependencies on macOS
install_deps_macos() {
    print_status "Installing dependencies on macOS..."
    
    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        print_error "Homebrew not found. Please install Homebrew first:"
        echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi
    
    # Install required packages
    brew install cmake pkg-config
    brew install boost
    brew install mongo-c-driver
    brew install libpq
    brew install fmt
    brew install spdlog
    brew install nlohmann-json
    brew install googletest
    
    # Set up environment variables for libpq
    print_warning "Setting up PostgreSQL environment variables..."
    echo 'export PKG_CONFIG_PATH="/opt/homebrew/opt/libpq/lib/pkgconfig:$PKG_CONFIG_PATH"' >> ~/.zshrc
    echo 'export LDFLAGS="-L/opt/homebrew/opt/libpq/lib"' >> ~/.zshrc
    echo 'export CPPFLAGS="-I/opt/homebrew/opt/libpq/include"' >> ~/.zshrc
    
    print_success "Dependencies installed on macOS"
    print_warning "Please restart your terminal or run: source ~/.zshrc"
}

# Function to install dependencies on Windows
install_deps_windows() {
    print_status "Installing dependencies on Windows..."
    
    # Check if vcpkg is installed
    if [ ! -d "vcpkg" ]; then
        print_status "Installing vcpkg..."
        git clone https://github.com/Microsoft/vcpkg.git
        cd vcpkg
        ./bootstrap-vcpkg.sh
        cd ..
    fi
    
    # Install required packages
    ./vcpkg/vcpkg install boost fmt spdlog asio abseil range-v3 nlohmann-json gtest
    
    print_success "Dependencies installed on Windows"
}

# Function to install dependencies automatically
install_dependencies() {
    if [ "$AUTO_INSTALL_DEPS" = false ]; then
        print_warning "Auto-installation of dependencies is disabled"
        return
    fi
    
    local os=$(detect_os)
    print_status "Detected OS: $os"
    
    case $os in
        "ubuntu")
            install_deps_ubuntu
            ;;
        "centos")
            install_deps_centos
            ;;
        "fedora")
            install_deps_fedora
            ;;
        "arch")
            install_deps_arch
            ;;
        "macos")
            install_deps_macos
            ;;
        "windows")
            install_deps_windows
            ;;
        *)
            print_warning "Unknown OS: $os. Please install dependencies manually."
            print_warning "Required packages: cmake, boost, mongo-c-driver, postgresql, fmt, spdlog, nlohmann-json, gtest"
            ;;
    esac
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -d, --debug             Build in debug mode"
    echo "  -r, --release           Build in release mode (default)"
    echo "  -t, --tests             Build with tests enabled"
    echo "  -c, --clean             Clean build directory before building"
    echo "  -v, --verbose           Verbose output"
    echo "  -i, --install DIR       Install to specified directory"
    echo "  -j, --jobs N            Number of parallel jobs (default: auto)"
    echo "  --no-deps               Skip automatic dependency installation"
    echo ""
    echo "Examples:"
    echo "  $0                     # Build in release mode with auto-deps"
    echo "  $0 -d -t              # Build in debug mode with tests"
    echo "  $0 -c -v              # Clean build with verbose output"
    echo "  $0 -i /usr/local      # Build and install to /usr/local"
    echo "  $0 --no-deps          # Build without installing dependencies"
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    local missing_deps=()
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi
    
    # Check CMake version
    if command -v cmake &> /dev/null; then
        local cmake_version=$(cmake --version | head -n1 | cut -d' ' -f3)
        local required_version="3.16.0"
        
        if [ "$(printf '%s\n' "$required_version" "$cmake_version" | sort -V | head -n1)" != "$required_version" ]; then
            missing_deps+=("cmake >= $required_version (found: $cmake_version)")
        fi
    fi
    
    # Check for C++ compiler
    if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
        missing_deps+=("C++ compiler (g++ or clang++)")
    fi
    
    # Check for required libraries
    if ! pkg-config --exists libpq 2>/dev/null; then
        missing_deps+=("PostgreSQL development headers (libpq)")
    fi
    
    if ! pkg-config --exists mongoc2 2>/dev/null; then
        missing_deps+=("libmongoc (mongo-c-driver)")
    fi
    
    if ! pkg-config --exists fmt 2>/dev/null; then
        missing_deps+=("fmt library")
    fi
    
    if ! pkg-config --exists spdlog 2>/dev/null; then
        missing_deps+=("spdlog library")
    fi
    
    if ! pkg-config --exists nlohmann_json 2>/dev/null; then
        missing_deps+=("nlohmann/json library")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_warning "Missing dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        
        if [ "$AUTO_INSTALL_DEPS" = true ]; then
            print_status "Attempting to install missing dependencies..."
            install_dependencies
        else
            print_error "Please install missing dependencies manually or run with --no-deps to skip this check"
            exit 1
        fi
    else
        print_success "All dependencies are available"
    fi
}

# Function to clean build directory
clean_build() {
    if [ "$CLEAN_BUILD" = true ]; then
        print_status "Cleaning build directory..."
        if [ -d "$BUILD_DIR" ]; then
            rm -rf "$BUILD_DIR"
            print_success "Build directory cleaned"
        fi
    fi
}

# Function to create build directory
create_build_dir() {
    print_status "Creating build directory..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
}

# Function to configure with CMake
configure_cmake() {
    print_status "Configuring with CMake..."

    # Always remove CMake cache and files to avoid path issues
    if [ -f "CMakeCache.txt" ]; then
        rm -f "CMakeCache.txt"
    fi
    if [ -d "CMakeFiles" ]; then
        rm -rf "CMakeFiles"
    fi

    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        "-DBUILD_TESTS=$BUILD_TESTS"
    )

    if [ -n "$INSTALL_DIR" ]; then
        cmake_args+=("-DCMAKE_INSTALL_PREFIX=$INSTALL_DIR")
    fi

    if [ "$VERBOSE" = true ]; then
        cmake_args+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
    fi

    cmake "${cmake_args[@]}" ..

    if [ $? -eq 0 ]; then
        print_success "CMake configuration completed"
    else
        print_error "CMake configuration failed"
        exit 1
    fi
}

# Function to build the project
build_project() {
    print_status "Building project..."
    
    local make_args=()
    
    if [ -n "$JOBS" ]; then
        make_args+=("-j$JOBS")
    else
        # Auto-detect number of CPU cores
        local cores=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
        make_args+=("-j$cores")
    fi
    
    if [ "$VERBOSE" = true ]; then
        make_args+=("VERBOSE=1")
    fi
    
    make "${make_args[@]}"
    
    if [ $? -eq 0 ]; then
        print_success "Build completed successfully"
    else
        print_error "Build failed"
        exit 1
    fi
}

# Function to run tests
run_tests() {
    if [ "$BUILD_TESTS" = "ON" ]; then
        print_status "Running tests..."
        make test
        
        if [ $? -eq 0 ]; then
            print_success "All tests passed"
        else
            print_warning "Some tests failed"
        fi
    fi
}

# Function to install
install_project() {
    if [ -n "$INSTALL_DIR" ]; then
        print_status "Installing to $INSTALL_DIR..."
        make install
        
        if [ $? -eq 0 ]; then
            print_success "Installation completed"
        else
            print_error "Installation failed"
            exit 1
        fi
    fi
}

# Function to show build summary
show_summary() {
    print_success "Build completed successfully!"
    echo ""
    echo "Build Summary:"
    echo "  Build Type: $BUILD_TYPE"
    echo "  Build Directory: $BUILD_DIR"
    echo "  Tests Enabled: $BUILD_TESTS"
    if [ -n "$INSTALL_DIR" ]; then
        echo "  Install Directory: $INSTALL_DIR"
    fi
    echo ""
    echo "Next steps:"
    echo "  - Run: ./$BUILD_DIR/fauxdb --help"
    echo "  - Copy config files: sudo cp config/fauxdb.conf /etc/fauxdb/"
    echo "  - Start service: sudo systemctl start fauxdb"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -t|--tests)
            BUILD_TESTS="ON"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -i|--install)
            INSTALL_DIR="$2"
            shift 2
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        --no-deps)
            AUTO_INSTALL_DEPS=false
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main build process
main() {
    print_status "Starting FauxDB build process..."
    echo ""
    
    # Install dependencies if needed
    if [ "$AUTO_INSTALL_DEPS" = true ]; then
        install_dependencies
    fi
    
    # Check dependencies
    check_dependencies
    
    # Clean build if requested
    clean_build
    
    # Create and enter build directory
    create_build_dir
    
    # Configure with CMake
    configure_cmake
    
    # Build the project
    build_project
    
    # Run tests if enabled
    run_tests
    
    # Install if requested
    install_project
    
    # Return to original directory
    cd ..
    
    # Show summary
    show_summary
}

# Run main function
main "$@"
