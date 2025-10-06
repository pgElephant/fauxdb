#!/bin/bash

# MongoDB Shell Test Runner for FauxDB
# This script runs comprehensive mongosh tests against both FauxDB and MongoDB

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
FAUXDB_HOST="localhost"
FAUXDB_PORT="27018"
MONGODB_HOST="localhost"
MONGODB_PORT="27017"
TEST_DB="test"

# Check if mongosh is installed
check_mongosh() {
    if ! command -v mongosh &> /dev/null; then
        echo -e "${RED}❌ mongosh is not installed or not in PATH${NC}"
        echo "Please install mongosh: https://docs.mongodb.com/mongodb-shell/install/"
        exit 1
    fi
    echo -e "${GREEN}✅ mongosh found: $(mongosh --version)${NC}"
}

# Check if services are running
check_services() {
    echo -e "\n${BLUE}🔍 Checking service availability...${NC}"
    
    # Check FauxDB
    if nc -z $FAUXDB_HOST $FAUXDB_PORT 2>/dev/null; then
        echo -e "${GREEN}✅ FauxDB is running on $FAUXDB_HOST:$FAUXDB_PORT${NC}"
    else
        echo -e "${YELLOW}⚠️ FauxDB is not running on $FAUXDB_HOST:$FAUXDB_PORT${NC}"
        echo "Please start FauxDB server before running tests"
    fi
    
    # Check MongoDB
    if nc -z $MONGODB_HOST $MONGODB_PORT 2>/dev/null; then
        echo -e "${GREEN}✅ MongoDB is running on $MONGODB_HOST:$MONGODB_PORT${NC}"
    else
        echo -e "${YELLOW}⚠️ MongoDB is not running on $MONGODB_HOST:$MONGODB_PORT${NC}"
        echo "Please start MongoDB server before running comparison tests"
    fi
}

# Test connection to a service
test_connection() {
    local host=$1
    local port=$2
    local name=$3
    local connection_string="mongodb://$host:$port/$TEST_DB"
    
    echo -e "\n${BLUE}🔌 Testing $name connection...${NC}"
    
    if mongosh --quiet --eval "db.adminCommand('ping')" "$connection_string" &>/dev/null; then
        echo -e "${GREEN}✅ $name connection successful${NC}"
        return 0
    else
        echo -e "${RED}❌ $name connection failed${NC}"
        return 1
    fi
}

# Run a specific test file
run_test() {
    local test_file=$1
    local test_name=$2
    local connection_string=$3
    
    echo -e "\n${BLUE}🧪 Running $test_name...${NC}"
    echo "Test file: $test_file"
    echo "Connection: $connection_string"
    
    if mongosh --quiet "$connection_string" "$test_file" 2>&1; then
        echo -e "${GREEN}✅ $test_name completed successfully${NC}"
        return 0
    else
        echo -e "${RED}❌ $test_name failed${NC}"
        return 1
    fi
}

# Main test execution
main() {
    echo -e "${BLUE}🚀 FauxDB MongoDB Shell Test Runner${NC}"
    echo "=========================================="
    
    # Check prerequisites
    check_mongosh
    check_services
    
    # Test connections
    local fauxdb_available=false
    local mongodb_available=false
    
    if test_connection $FAUXDB_HOST $FAUXDB_PORT "FauxDB"; then
        fauxdb_available=true
    fi
    
    if test_connection $MONGODB_HOST $MONGODB_PORT "MongoDB"; then
        mongodb_available=true
    fi
    
    # Get script directory
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    
    # Run tests based on availability
    local tests_passed=0
    local tests_failed=0
    
    if [ "$fauxdb_available" = true ]; then
        echo -e "\n${BLUE}📊 Running FauxDB-specific tests...${NC}"
        
        # Run PostgreSQL backend tests
        if run_test "$SCRIPT_DIR/postgresql_tests.js" "PostgreSQL Backend Tests" "mongodb://$FAUXDB_HOST:$FAUXDB_PORT/$TEST_DB"; then
            ((tests_passed++))
        else
            ((tests_failed++))
        fi
        
        # Run general mongosh tests (FauxDB only)
        if run_test "$SCRIPT_DIR/mongosh_tests.js" "General MongoDB Shell Tests" "mongodb://$FAUXDB_HOST:$FAUXDB_PORT/$TEST_DB"; then
            ((tests_passed++))
        else
            ((tests_failed++))
        fi
    else
        echo -e "${YELLOW}⚠️ Skipping FauxDB tests - service not available${NC}"
    fi
    
    if [ "$fauxdb_available" = true ] && [ "$mongodb_available" = true ]; then
        echo -e "\n${BLUE}⚖️ Running comparison tests...${NC}"
        
        # Run MongoDB vs FauxDB comparison tests
        if run_test "$SCRIPT_DIR/mongodb_comparison_tests.js" "MongoDB vs FauxDB Comparison" "mongodb://$FAUXDB_HOST:$FAUXDB_PORT/$TEST_DB"; then
            ((tests_passed++))
        else
            ((tests_failed++))
        fi
    else
        echo -e "${YELLOW}⚠️ Skipping comparison tests - both services not available${NC}"
    fi
    
    # Print final results
    echo -e "\n${BLUE}🏁 Test Results Summary${NC}"
    echo "========================"
    echo -e "${GREEN}✅ Tests Passed: $tests_passed${NC}"
    echo -e "${RED}❌ Tests Failed: $tests_failed${NC}"
    
    local total_tests=$((tests_passed + tests_failed))
    if [ $total_tests -gt 0 ]; then
        local success_rate=$((tests_passed * 100 / total_tests))
        echo -e "${BLUE}🎯 Success Rate: $success_rate%${NC}"
    fi
    
    if [ $tests_failed -eq 0 ]; then
        echo -e "\n${GREEN}🎉 All tests passed! FauxDB is working correctly.${NC}"
        exit 0
    else
        echo -e "\n${RED}💥 Some tests failed. Please check the output above.${NC}"
        exit 1
    fi
}

# Help function
show_help() {
    echo "FauxDB MongoDB Shell Test Runner"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -f, --fauxdb   Test only FauxDB (skip MongoDB comparison)"
    echo "  -m, --mongodb  Test only MongoDB comparison (requires both services)"
    echo "  --fauxdb-host  FauxDB host (default: localhost)"
    echo "  --fauxdb-port  FauxDB port (default: 27018)"
    echo "  --mongodb-host MongoDB host (default: localhost)"
    echo "  --mongodb-port MongoDB port (default: 27017)"
    echo ""
    echo "Examples:"
    echo "  $0                    # Run all available tests"
    echo "  $0 -f                 # Test only FauxDB"
    echo "  $0 --fauxdb-port 27019 # Use custom FauxDB port"
    echo ""
    echo "Prerequisites:"
    echo "  - mongosh installed and in PATH"
    echo "  - FauxDB server running (default: localhost:27018)"
    echo "  - MongoDB server running for comparison tests (default: localhost:27017)"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -f|--fauxdb)
            FAUXDB_ONLY=true
            shift
            ;;
        -m|--mongodb)
            MONGODB_ONLY=true
            shift
            ;;
        --fauxdb-host)
            FAUXDB_HOST="$2"
            shift 2
            ;;
        --fauxdb-port)
            FAUXDB_PORT="$2"
            shift 2
            ;;
        --mongodb-host)
            MONGODB_HOST="$2"
            shift 2
            ;;
        --mongodb-port)
            MONGODB_PORT="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Run main function
main
