#!/bin/bash

# Document Database Client Regressio# Set default client if not set
if [ -z "$CLIENT" ]; then
    export CLIENT=mongosh
fi

# Check if document database shell is available
check_client_shell() {
    if ! command -v $CLIENT &> /dev/null; then
        log_error "$CLIENT is not installed. Please install document database shell."
        log_info "Install with: brew install mongosh"
        return 1
    fi
    log_info "Found client shell: $(which $CLIENT)"
}ript
# This script runs fauxdb and tests it with actual document database client queries

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
REGRESSION_DIR="$PROJECT_ROOT/regression"
EXPECTED_DIR="$REGRESSION_DIR/expected"
ACTUAL_DIR="$REGRESSION_DIR/actual"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
FAUXDB_PORT=27018
FAUXDB_HOST="127.0.0.1"
TEST_DATABASE="testdb"

# Global variables
FAUXDB_PID=""
TESTS_PASSED=0
TESTS_FAILED=0
TOTAL_TESTS=0

# Utility functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if $CLIENT is available
check_mongosh() {
    if ! command -v $CLIENT &> /dev/null; then
        log_error "$CLIENT is not installed. Please install MongoDB Shell."
        log_info "Install with: brew install mongosh"
        exit 1
    fi
    log_info "Found mongosh: $(which mongosh)"
}

# Check if fauxdb binary exists
check_fauxdb() {
    if [ ! -f "$BUILD_DIR/fauxdb" ]; then
        log_error "fauxdb binary not found at $BUILD_DIR/fauxdb"
        log_info "Please build fauxdb first: ./build.sh"
        exit 1
    fi
    log_info "Found fauxdb: $BUILD_DIR/fauxdb"
}

# Start fauxdb server
start_fauxdb() {
    log_info "Starting fauxdb server on port $FAUXDB_PORT..."
    
    # Create logs directory
    mkdir -p "$REGRESSION_DIR/logs"
    
    # Start fauxdb in background
    cd "$BUILD_DIR"
    nohup ./fauxdb > "$REGRESSION_DIR/logs/fauxdb.log" 2>&1 &
    FAUXDB_PID=$!
    cd - > /dev/null
    
    log_info "Started fauxdb with PID: $FAUXDB_PID"
    
    # Wait for server to start
    log_info "Waiting for server to start..."
    sleep 3
    
    # Check if server is running
    if ! kill -0 $FAUXDB_PID 2>/dev/null; then
        log_error "Failed to start fauxdb server"
        cat "$REGRESSION_DIR/logs/fauxdb.log"
        exit 1
    fi
    
    log_success "FauxDB server started successfully"
}

# Stop fauxdb server
stop_fauxdb() {
    if [ ! -z "$FAUXDB_PID" ]; then
        log_info "Stopping fauxdb server (PID: $FAUXDB_PID)..."
        kill $FAUXDB_PID 2>/dev/null || true
        wait $FAUXDB_PID 2>/dev/null || true
        log_success "FauxDB server stopped"
    fi
}

# Create actual results directory
setup_results_dir() {
    rm -rf "$ACTUAL_DIR"
    mkdir -p "$ACTUAL_DIR"
    log_info "Created actual results directory: $ACTUAL_DIR"
}

# Test server connectivity
test_connectivity() {
    log_info "Testing server connectivity..."
    
    local connection_test=$($CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT --eval "print('connection test')" --quiet 2>/dev/null || echo "failed")
    
    if [[ "$connection_test" == *"connection test"* ]]; then
        log_success "Server connectivity test passed"
        return 0
    else
        log_error "Cannot connect to fauxdb server"
        return 1
    fi
}

# Execute a MongoDB command and capture output
execute_mongo_command() {
    local test_name="$1"
    local command="$2"
    local expected_file="$3"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    log_info "Running test: $test_name"
    
    # Execute command and capture output
    local actual_file="$ACTUAL_DIR/${test_name}_actual.json"
    
    # Use $CLIENT to execute command
    local mongo_script="
        try {
            let result = $command;
            print(JSON.stringify(result, null, 2));
        } catch (error) {
            print(JSON.stringify({error: error.message}, null, 2));
        }
    "
    
    echo "$mongo_script" | $CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT $TEST_DATABASE --quiet > "$actual_file" 2>&1
    
    # Clean up $CLIENT output (remove shell messages)
    sed -i '' '/^Current Mongosh Log ID:/d' "$actual_file" 2>/dev/null || true
    sed -i '' '/^Connecting to:/d' "$actual_file" 2>/dev/null || true
    sed -i '' '/^Using MongoDB:/d' "$actual_file" 2>/dev/null || true
    sed -i '' '/^For $CLIENT info see:/d' "$actual_file" 2>/dev/null || true
    sed -i '' '/^test>/d' "$actual_file" 2>/dev/null || true
    sed -i '' '/^testdb>/d' "$actual_file" 2>/dev/null || true
    sed -i '' '/^---$/d' "$actual_file" 2>/dev/null || true
    sed -i '' '/^$/d' "$actual_file" 2>/dev/null || true
    
    # Compare with expected result
    if [ -f "$expected_file" ]; then
        if diff -q "$actual_file" "$expected_file" > /dev/null; then
            log_success "Test $test_name: PASSED"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            log_error "Test $test_name: FAILED"
            log_info "Expected result:"
            cat "$expected_file" | head -10
            log_info "Actual result:"
            cat "$actual_file" | head -10
            log_info "Full diff:"
            diff "$expected_file" "$actual_file" || true
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        log_warning "Expected result file not found: $expected_file"
        log_info "Actual result saved to: $actual_file"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Individual test functions
test_ping() {
    execute_mongo_command "ping" \
        "db.runCommand({ping: 1})" \
        "$EXPECTED_DIR/ping_expected.json"
}

test_list_databases() {
    execute_mongo_command "list_databases" \
        "db.adminCommand({listDatabases: 1})" \
        "$EXPECTED_DIR/list_databases_expected.json"
}

test_insert_document() {
    execute_mongo_command "insert_document" \
        "db.users.insertOne({name: 'John Doe', age: 30, email: 'john@example.com'})" \
        "$EXPECTED_DIR/insert_document_expected.json"
}

test_find_documents() {
    # First insert test data
    $CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT $TEST_DATABASE --eval "
        db.users.insertOne({name: 'Jane Doe', age: 28, email: 'jane@example.com'});
    " --quiet > /dev/null 2>&1
    
    execute_mongo_command "find_documents" \
        "db.users.find({age: {\$gt: 25}}).toArray()" \
        "$EXPECTED_DIR/find_documents_expected.json"
}

test_update_document() {
    # First insert test data
    $CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT $TEST_DATABASE --eval "
        db.users.insertOne({name: 'Update Test', age: 35, status: 'active'});
    " --quiet > /dev/null 2>&1
    
    execute_mongo_command "update_document" \
        "db.users.updateOne({name: 'Update Test'}, {\$set: {age: 36}})" \
        "$EXPECTED_DIR/update_document_expected.json"
}

test_delete_document() {
    # First insert test data
    $CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT $TEST_DATABASE --eval "
        db.users.insertOne({name: 'Delete Test', temporary: true});
    " --quiet > /dev/null 2>&1
    
    execute_mongo_command "delete_document" \
        "db.users.deleteOne({name: 'Delete Test'})" \
        "$EXPECTED_DIR/delete_document_expected.json"
}

test_count_documents() {
    # Insert some test data
    $CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT $TEST_DATABASE --eval "
        db.test_collection.insertOne({category: 'test', value: 1});
        db.test_collection.insertOne({category: 'test', value: 2});
    " --quiet > /dev/null 2>&1
    
    execute_mongo_command "count_documents" \
        "db.test_collection.countDocuments({category: 'test'})" \
        "$EXPECTED_DIR/count_documents_expected.json"
}

test_complex_query() {
    # Insert complex test data
    $CLIENT --host $FAUXDB_HOST --port $FAUXDB_PORT $TEST_DATABASE --eval "
        db.employees.insertOne({
            name: 'Alice',
            age: 30,
            skills: ['python', 'sql'],
            address: {city: 'San Francisco', state: 'CA'}
        });
    " --quiet > /dev/null 2>&1
    
    execute_mongo_command "complex_query" \
        "db.employees.find({age: {\$gte: 25, \$lte: 35}, 'address.state': 'CA'}).toArray()" \
        "$EXPECTED_DIR/complex_query_expected.json"
}

# Run all tests
run_all_tests() {
    log_info "Running all regression tests..."
    
    test_ping
    test_list_databases
    test_insert_document
    test_find_documents
    test_update_document
    test_delete_document
    test_count_documents
    test_complex_query
    
    # Print summary
    echo
    echo "======================================"
    echo "         TEST SUMMARY"
    echo "======================================"
    echo "Total Tests:  $TOTAL_TESTS"
    echo "Passed:       $TESTS_PASSED"
    echo "Failed:       $TESTS_FAILED"
    echo "======================================"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        log_success "All tests passed!"
        return 0
    else
        log_error "$TESTS_FAILED tests failed"
        return 1
    fi
}

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    stop_fauxdb
}

# Main execution
main() {
    echo "======================================"
    echo "    FauxDB MongoDB Regression Tests"
    echo "======================================"
    
    # Set up cleanup trap
    trap cleanup EXIT
    
    # Check prerequisites
    check_client_shell
    check_fauxdb
    
    # Setup
    setup_results_dir
    start_fauxdb
    
    # Test connectivity
    if ! test_connectivity; then
        log_error "Server connectivity test failed"
        exit 1
    fi
    
    # Run tests
    run_all_tests
    
    # Exit with appropriate code
    if [ $TESTS_FAILED -eq 0 ]; then
        exit 0
    else
        exit 1
    fi
}

# Run main function
main "$@"
