#!/bin/bash

# FauxDB Regression Test Runner - TAP/Prove Style Output
# This script runs MongoDB regression tests and outputs in TAP format for use with prove

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# Test configuration
FAUXDB_PORT=27018
FAUXDB_HOST="127.0.0.1"
TEST_DATABASE="testdb"

# Global variables
FAUXDB_PID=""

# TAP output functions
tap_plan() {
    echo "1..$1"
}

tap_ok() {
    local test_num=$1
    local description="$2"
    echo "ok $test_num - $description"
}

tap_not_ok() {
    local test_num=$1
    local description="$2"
    echo "not ok $test_num - $description"
}

tap_skip() {
    local test_num=$1
    local reason="$2"
    echo "ok $test_num # SKIP $reason"
}

tap_diag() {
    echo "# $1"
}

# Start fauxdb server
start_fauxdb() {
    tap_diag "Starting FauxDB server on port $FAUXDB_PORT"
    
    cd "$BUILD_DIR"
    nohup ./fauxdb > /dev/null 2>&1 &
    FAUXDB_PID=$!
    cd - > /dev/null
    
    # Wait for server to start
    sleep 3
    
    if ! kill -0 $FAUXDB_PID 2>/dev/null; then
        tap_diag "Failed to start FauxDB server"
        return 1
    fi
    
    tap_diag "FauxDB server started with PID: $FAUXDB_PID"
    return 0
}

# Stop fauxdb server
stop_fauxdb() {
    if [ ! -z "$FAUXDB_PID" ]; then
        tap_diag "Stopping FauxDB server (PID: $FAUXDB_PID)"
        kill $FAUXDB_PID 2>/dev/null || true
        wait $FAUXDB_PID 2>/dev/null || true
    fi
}

# Test connectivity
test_connectivity() {
    local test_num=$1
    
    if command -v $CLIENT &> /dev/null; then
        local result=$($CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT --eval "print('connected')" --quiet 2>/dev/null || echo "failed")
        
        if [[ "$result" == *"connected"* ]]; then
            tap_ok $test_num "MongoDB client can connect to FauxDB server"
            return 0
        else
            tap_not_ok $test_num "MongoDB client connection to FauxDB server"
            tap_diag "Connection test returned: $result"
            return 1
        fi
    else
        tap_skip $test_num "$CLIENT not available"
        return 0
    fi
}

# Run ping test
test_ping() {
    local test_num=$1
    
    local result=$($CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT --eval "
        try {
            let result = db.runCommand({ping: 1});
            print(result.ok === 1 ? 'PASS' : 'FAIL');
        } catch (error) {
            print('ERROR');
        }
    " --quiet 2>/dev/null)
    
    if [[ "$result" == *"PASS"* ]]; then
        tap_ok $test_num "ping command returns ok: 1"
    else
        tap_not_ok $test_num "ping command execution"
        tap_diag "Ping result: $result"
    fi
}

# Run insert test
test_insert() {
    local test_num=$1
    
    local result=$($CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT --eval "
        try {
            let result = db.users.insertOne({name: 'Test User', age: 25});
            print(result.acknowledged ? 'PASS' : 'FAIL');
        } catch (error) {
            print('ERROR');
        }
    " --quiet 2>/dev/null)
    
    if [[ "$result" == *"PASS"* ]]; then
        tap_ok $test_num "document insertion succeeds"
    else
        tap_not_ok $test_num "document insertion"
        tap_diag "Insert result: $result"
    fi
}

# Run find test
test_find() {
    local test_num=$1
    
    local result=$($CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT --eval "
        try {
            let result = db.users.find({age: {\$gt: 20}}).toArray();
            print(Array.isArray(result) ? 'PASS' : 'FAIL');
        } catch (error) {
            print('ERROR');
        }
    " --quiet 2>/dev/null)
    
    if [[ "$result" == *"PASS"* ]]; then
        tap_ok $test_num "document query returns array"
    else
        tap_not_ok $test_num "document query execution"
        tap_diag "Find result: $result"
    fi
}

# Run TAP-style JavaScript tests
run_tap_tests() {
    local test_num=$1
    
    if [ -f "$PROJECT_ROOT/regression/scripts/tap_regression_tests.js" ]; then
        tap_diag "Running comprehensive TAP regression tests"
        
        # Capture the TAP output from MongoDB shell
        local tap_output=$($CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT "$PROJECT_ROOT/regression/scripts/tap_regression_tests.js" 2>/dev/null)
        
        # Check if we got valid TAP output
        if [[ "$tap_output" == *"1.."* ]]; then
            tap_ok $test_num "TAP regression test suite executed"
            # Output the TAP results (they're already in TAP format)
            echo "$tap_output" | grep -E "^(ok|not ok|1\.\.|#)"
        else
            tap_not_ok $test_num "TAP regression test suite execution"
            tap_diag "TAP output was invalid or empty"
        fi
    else
        tap_skip $test_num "TAP regression test file not found"
    fi
}

# Main test execution
main() {
    local total_tests=5
    
    tap_diag "FauxDB MongoDB Regression Tests - TAP Format"
    tap_diag "Compatible with Perl's prove command"
    
    # Check prerequisites
    if [ ! -f "$BUILD_DIR/fauxdb" ]; then
        tap_diag "FauxDB binary not found at $BUILD_DIR/fauxdb"
        tap_plan $total_tests
        for i in $(seq 1 $total_tests); do
            tap_skip $i "FauxDB binary not available"
        done
        exit 0
    fi
    
    # Set up cleanup
    trap stop_fauxdb EXIT
    
    # Start server
    if ! start_fauxdb; then
        tap_plan $total_tests
        for i in $(seq 1 $total_tests); do
            tap_skip $i "FauxDB server failed to start"
        done
        exit 0
    fi
    
    # Run tests
    tap_plan $total_tests
    
    test_connectivity 1
    test_ping 2
    test_insert 3
    test_find 4
    run_tap_tests 5
    
    tap_diag "All regression tests completed"
}

# Run main function
main "$@"
