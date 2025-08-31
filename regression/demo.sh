#!/bin/bash

# FauxDB Regression Test Demo
# This script demonstrates how to run the regression tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}"
echo "======================================"
echo "    FauxDB Regression Test Demo"
echo "======================================"
echo -e "${NC}"

echo -e "${YELLOW}This demo will:${NC}"
echo "1. Build FauxDB"
echo "2. Start the FauxDB server"
echo "3. Run MongoDB client regression tests"
echo "4. Show test results"
echo

# Check if mongosh is available
if ! command -v mongosh &> /dev/null; then
    echo -e "${RED}Error: mongosh (MongoDB Shell) is not installed.${NC}"
    echo "Please install it with: brew install mongosh"
    exit 1
fi

# Step 1: Build FauxDB
echo -e "${BLUE}Step 1: Building FauxDB...${NC}"
cd "$PROJECT_ROOT"
if [ ! -f "build.sh" ]; then
    echo -e "${RED}Error: build.sh not found in project root${NC}"
    exit 1
fi

./build.sh

if [ ! -f "build/fauxdb" ]; then
    echo -e "${RED}Error: FauxDB build failed${NC}"
    exit 1
fi

echo -e "${GREEN}✓ FauxDB built successfully${NC}"

# Step 2: Start FauxDB server
echo -e "${BLUE}Step 2: Starting FauxDB server...${NC}"

# Create logs directory
mkdir -p "$PROJECT_ROOT/regression/logs"

# Start FauxDB in background
cd "$PROJECT_ROOT/build"
nohup ./fauxdb > "$PROJECT_ROOT/regression/logs/fauxdb_demo.log" 2>&1 &
FAUXDB_PID=$!

echo -e "${GREEN}✓ FauxDB server started (PID: $FAUXDB_PID)${NC}"
echo "Server logs: $PROJECT_ROOT/regression/logs/fauxdb_demo.log"

# Cleanup function
cleanup() {
    echo -e "\n${BLUE}Cleaning up...${NC}"
    if [ ! -z "$FAUXDB_PID" ]; then
        kill $FAUXDB_PID 2>/dev/null || true
        echo -e "${GREEN}✓ FauxDB server stopped${NC}"
    fi
}

# Set cleanup trap
trap cleanup EXIT

# Wait for server to start
echo "Waiting for server to start..."
sleep 3

# Check if server is running
if ! kill -0 $FAUXDB_PID 2>/dev/null; then
    echo -e "${RED}Error: FauxDB server failed to start${NC}"
    echo "Check the logs:"
    cat "$PROJECT_ROOT/regression/logs/fauxdb_demo.log"
    exit 1
fi

# Test connectivity
echo "Testing server connectivity..."
CONNECTION_TEST=$(mongosh --host 127.0.0.1 --port 27018 --eval "print('connected')" --quiet 2>/dev/null || echo "failed")

if [[ "$CONNECTION_TEST" != *"connected"* ]]; then
    echo -e "${RED}Error: Cannot connect to FauxDB server${NC}"
    echo "Server logs:"
    tail -20 "$PROJECT_ROOT/regression/logs/fauxdb_demo.log"
    exit 1
fi

echo -e "${GREEN}✓ Server connectivity confirmed${NC}"

# Step 3: Run regression tests
echo -e "${BLUE}Step 3: Running MongoDB client regression tests...${NC}"
cd "$PROJECT_ROOT/regression/scripts"

echo "Executing: mongosh --host 127.0.0.1 --port 27018 regression_tests.js"
echo

# Run the JavaScript regression tests
mongosh --host 127.0.0.1 --port 27018 regression_tests.js

echo

# Step 4: Show additional information
echo -e "${BLUE}Step 4: Additional Test Information${NC}"

echo -e "${YELLOW}Available test scripts:${NC}"
echo "  - JavaScript tests: regression/scripts/regression_tests.js"
echo "  - Bash test runner: regression/scripts/run_regression_tests.sh"
echo "  - C++ tests: regression/integration/mongo_client_regression.cpp"

echo -e "${YELLOW}Test data locations:${NC}"
echo "  - Expected results: regression/expected/"
echo "  - Server logs: regression/logs/"

echo -e "${YELLOW}Manual test examples:${NC}"
echo "  # Connect to FauxDB with MongoDB shell:"
echo "  mongosh --host 127.0.0.1 --port 27018"
echo ""
echo "  # Run individual test:"
echo "  mongosh --host 127.0.0.1 --port 27018 --eval 'db.runCommand({ping: 1})'"

echo
echo -e "${GREEN}Demo completed successfully!${NC}"
echo -e "${BLUE}FauxDB is running and responding to MongoDB client requests.${NC}"
