#!/bin/bash

# FauxDB MongoDB Command Test Runner
# Comprehensive regression testing with expected vs actual output comparison

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuration
MONGO_PORT=27018
TEST_DB="fauxdb"
POSTGRES_HOST="localhost"
POSTGRES_PORT=5432
POSTGRES_USER="fauxdb"
POSTGRES_DB="fauxdb"

# Directories
TESTS_DIR="tests"
EXPECTED_DIR="tests/expected"
OUTPUT_DIR="tests/output"
SETUP_DIR="tests/setup"

# Counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

echo -e "${BLUE}=== FauxDB MongoDB Command Test Runner ===${NC}"
echo "Server: localhost:$MONGO_PORT"
echo "PostgreSQL: $POSTGRES_HOST:$POSTGRES_PORT/$POSTGRES_DB"
echo ""

# Create directory structure
setup_directories() {
    echo -e "${CYAN}Setting up test directories...${NC}"
    mkdir -p "$TESTS_DIR"
    mkdir -p "$EXPECTED_DIR"
    mkdir -p "$OUTPUT_DIR"
    mkdir -p "$SETUP_DIR"
    
    # Clean previous outputs
    rm -f "$OUTPUT_DIR"/*.json
    rm -f "$OUTPUT_DIR"/*.log
}

# Initialize PostgreSQL database
setup_postgresql() {
    echo -e "${CYAN}Setting up PostgreSQL database...${NC}"
    
    cat > "$SETUP_DIR/pg_setup.sql" << 'EOF'
-- FauxDB Test Database Setup
-- Drop and recreate tables for consistent testing

DROP TABLE IF EXISTS test CASCADE;
DROP TABLE IF EXISTS users CASCADE;
DROP TABLE IF EXISTS products CASCADE;
DROP TABLE IF EXISTS orders CASCADE;

-- Create test table with 100 rows (as expected by tests)
CREATE TABLE test (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    value INTEGER,
    active BOOLEAN DEFAULT true,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert 100 test records
INSERT INTO test (name, value, active) 
SELECT 
    'test_record_' || i,
    (i * 10) % 1000,
    (i % 2 = 0)
FROM generate_series(1, 100) i;

-- Create users table
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100),
    age INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO users (username, email, age) VALUES
    ('alice', 'alice@example.com', 25),
    ('bob', 'bob@example.com', 30),
    ('charlie', 'charlie@example.com', 35);

-- Create products table
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10,2),
    category VARCHAR(50),
    in_stock BOOLEAN DEFAULT true
);

INSERT INTO products (name, price, category, in_stock) VALUES
    ('Laptop', 999.99, 'electronics', true),
    ('Book', 19.99, 'books', true),
    ('Coffee', 4.99, 'food', false);

-- Verify setup
SELECT 'test' as table_name, COUNT(*) as row_count FROM test
UNION ALL
SELECT 'users', COUNT(*) FROM users  
UNION ALL
SELECT 'products', COUNT(*) FROM products;
EOF

    # Execute PostgreSQL setup
    if psql -h "$POSTGRES_HOST" -p "$POSTGRES_PORT" -U "$POSTGRES_USER" -d "$POSTGRES_DB" -f "$SETUP_DIR/pg_setup.sql" > "$OUTPUT_DIR/pg_setup.log" 2>&1; then
        echo -e "${GREEN}PostgreSQL setup completed${NC}"
    else
        echo -e "${RED}PostgreSQL setup failed. Check $OUTPUT_DIR/pg_setup.log${NC}"
        exit 1
    fi
}

# Check if FauxDB server is running
check_server() {
    echo -e "${CYAN}Checking FauxDB server...${NC}"
    
    for i in {1..5}; do
        if mongosh --port "$MONGO_PORT" --quiet --eval "db.runCommand({ping: 1})" > /dev/null 2>&1; then
            echo -e "${GREEN}FauxDB server is running on port $MONGO_PORT${NC}"
            return 0
        fi
        echo "Attempt $i: Server not responding, waiting..."
        sleep 2
    done
    
    echo -e "${RED}FauxDB server is not running on port $MONGO_PORT${NC}"
    echo "Please start the server with: ./fauxdb --config config/fauxdb.json"
    exit 1
}

# Generate test cases
generate_tests() {
    echo -e "${CYAN}Generating test cases...${NC}"
    
    # Basic Admin Commands
    cat > "$TESTS_DIR/ping.js" << 'EOF'
db.runCommand({ping: 1})
EOF

    cat > "$EXPECTED_DIR/ping.json" << 'EOF'
{
  "ok": 1
}
EOF

    cat > "$TESTS_DIR/hello.js" << 'EOF'
db.runCommand({hello: 1})
EOF

    cat > "$EXPECTED_DIR/hello.json" << 'EOF'
{
  "ok": 1,
  "isWritablePrimary": true,
  "ismaster": true,
  "maxBsonObjectSize": 16777216
}
EOF

    cat > "$TESTS_DIR/buildInfo.js" << 'EOF'
db.runCommand({buildInfo: 1})
EOF

    cat > "$EXPECTED_DIR/buildInfo.json" << 'EOF'
{
  "ok": 1,
  "version": "1.0.0",
  "gitVersion": "fauxdb-1.0.0"
}
EOF

    # Read Commands
    cat > "$TESTS_DIR/find_basic.js" << 'EOF'
db.runCommand({find: "test", limit: 5})
EOF

    cat > "$EXPECTED_DIR/find_basic.json" << 'EOF'
{
  "ok": 1,
  "cursor": {
    "id": 0,
    "ns": "test.collection",
    "firstBatch": [
      {"_id": "pg_1"},
      {"_id": "pg_2"},
      {"_id": "pg_3"},
      {"_id": "pg_4"},
      {"_id": "pg_5"}
    ]
  }
}
EOF

    cat > "$TESTS_DIR/findOne.js" << 'EOF'
db.runCommand({findOne: "test"})
EOF

    cat > "$EXPECTED_DIR/findOne.json" << 'EOF'
{
  "ok": 1,
  "_id": "pg_1"
}
EOF

    cat > "$TESTS_DIR/count.js" << 'EOF'
db.runCommand({count: "test"})
EOF

    cat > "$EXPECTED_DIR/count.json" << 'EOF'
{
  "ok": 1,
  "n": 100
}
EOF

    cat > "$TESTS_DIR/countDocuments.js" << 'EOF'
db.runCommand({countDocuments: "test"})
EOF

    cat > "$EXPECTED_DIR/countDocuments.json" << 'EOF'
{
  "ok": 1,
  "n": 100
}
EOF

    # Write Commands (should show not implemented)
    cat > "$TESTS_DIR/insert.js" << 'EOF'
db.runCommand({insert: "test", documents: [{name: "new_item", value: 999}]})
EOF

    cat > "$EXPECTED_DIR/insert.json" << 'EOF'
{
  "ok": 1,
  "n": 0,
  "errmsg": "Insert not implemented - PostgreSQL integration pending"
}
EOF

    cat > "$TESTS_DIR/update.js" << 'EOF'
db.runCommand({update: "test", updates: [{q: {name: "test"}, u: {$set: {updated: true}}}]})
EOF

    cat > "$EXPECTED_DIR/update.json" << 'EOF'
{
  "ok": 1,
  "n": 0,
  "nModified": 0,
  "errmsg": "Update not implemented - PostgreSQL integration pending"
}
EOF

    # Database Management Commands
    cat > "$TESTS_DIR/listCollections.js" << 'EOF'
db.runCommand({listCollections: 1})
EOF

    cat > "$EXPECTED_DIR/listCollections.json" << 'EOF'
{
  "ok": 1,
  "cursor": {
    "id": 0,
    "ns": "admin.$cmd.listCollections",
    "firstBatch": [
      {
        "name": "test",
        "type": "collection"
      }
    ]
  }
}
EOF

    cat > "$TESTS_DIR/listDatabases.js" << 'EOF'
db.runCommand({listDatabases: 1})
EOF

    cat > "$EXPECTED_DIR/listDatabases.json" << 'EOF'
{
  "ok": 1,
  "databases": [
    {
      "name": "test",
      "sizeOnDisk": 1024,
      "empty": false
    }
  ],
  "totalSize": 1024
}
EOF

    # Aggregation Commands
    cat > "$TESTS_DIR/aggregate_empty.js" << 'EOF'
db.runCommand({aggregate: "test", pipeline: []})
EOF

    cat > "$EXPECTED_DIR/aggregate_empty.json" << 'EOF'
{
  "ok": 1,
  "cursor": {
    "id": 0,
    "ns": "test.collection",
    "firstBatch": []
  }
}
EOF

    # Error Cases
    cat > "$TESTS_DIR/unknown_command.js" << 'EOF'
db.runCommand({unknownCommand: 1})
EOF

    cat > "$EXPECTED_DIR/unknown_command.json" << 'EOF'
{
  "ok": 0,
  "code": 59,
  "codeName": "CommandNotFound",
  "errmsg": "no such command: unknownCommand"
}
EOF

    echo -e "${GREEN}Test cases generated${NC}"
}

# Run a single test
run_test() {
    local test_name="$1"
    local test_file="$TESTS_DIR/${test_name}.js"
    local expected_file="$EXPECTED_DIR/${test_name}.json"
    local output_file="$OUTPUT_DIR/${test_name}.json"
    local diff_file="$OUTPUT_DIR/${test_name}.diff"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo -n "Testing $test_name... "
    
    # Run the MongoDB command
    if mongosh --port "$MONGO_PORT" --quiet --eval "$(cat "$test_file")" > "$output_file" 2>&1; then
        # Compare with expected output using jq for JSON normalization
        if command -v jq >/dev/null 2>&1; then
            # Normalize JSON and compare
            if jq -S . "$expected_file" > "$expected_file.norm" 2>/dev/null && \
               jq -S . "$output_file" > "$output_file.norm" 2>/dev/null; then
                if diff -u "$expected_file.norm" "$output_file.norm" > "$diff_file" 2>&1; then
                    echo -e "${GREEN}PASS${NC}"
                    PASSED_TESTS=$((PASSED_TESTS + 1))
                    rm -f "$diff_file" "$expected_file.norm" "$output_file.norm"
                else
                    echo -e "${RED}FAIL${NC} - Output differs"
                    FAILED_TESTS=$((FAILED_TESTS + 1))
                    echo "  Expected: $expected_file"
                    echo "  Actual:   $output_file"
                    echo "  Diff:     $diff_file"
                    rm -f "$expected_file.norm" "$output_file.norm"
                fi
            else
                # Fallback to simple text comparison if jq fails
                if diff -u "$expected_file" "$output_file" > "$diff_file" 2>&1; then
                    echo -e "${GREEN}PASS${NC}"
                    PASSED_TESTS=$((PASSED_TESTS + 1))
                    rm -f "$diff_file"
                else
                    echo -e "${RED}FAIL${NC} - Output differs (text)"
                    FAILED_TESTS=$((FAILED_TESTS + 1))
                fi
            fi
        else
            # No jq available, use simple diff
            if diff -u "$expected_file" "$output_file" > "$diff_file" 2>&1; then
                echo -e "${GREEN}PASS${NC}"
                PASSED_TESTS=$((PASSED_TESTS + 1))
                rm -f "$diff_file"
            else
                echo -e "${RED}FAIL${NC} - Output differs"
                FAILED_TESTS=$((FAILED_TESTS + 1))
            fi
        fi
    else
        echo -e "${RED}FAIL${NC} - Command execution failed"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
}

# Run all tests
run_all_tests() {
    echo -e "\n${YELLOW}=== Running All Tests ===${NC}"
    
    # Get all test files
    local test_files=($(ls "$TESTS_DIR"/*.js 2>/dev/null | xargs -n1 basename | sed 's/\.js$//' | sort))
    
    if [ ${#test_files[@]} -eq 0 ]; then
        echo -e "${RED}No test files found in $TESTS_DIR${NC}"
        exit 1
    fi
    
    echo "Found ${#test_files[@]} test cases"
    echo ""
    
    # Run each test
    for test_name in "${test_files[@]}"; do
        run_test "$test_name"
    done
}

# Generate test report
generate_report() {
    local report_file="$OUTPUT_DIR/test_report.html"
    local timestamp=$(date)
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>FauxDB Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background: #f0f0f0; padding: 10px; border-radius: 5px; }
        .pass { color: green; }
        .fail { color: red; }
        .summary { background: #e8f4f8; padding: 15px; margin: 20px 0; border-radius: 5px; }
        pre { background: #f5f5f5; padding: 10px; border-radius: 3px; overflow-x: auto; }
    </style>
</head>
<body>
    <div class="header">
        <h1>FauxDB MongoDB Command Test Report</h1>
        <p>Generated: $timestamp</p>
        <p>Server: localhost:$MONGO_PORT</p>
    </div>
    
    <div class="summary">
        <h2>Summary</h2>
        <p>Total Tests: $TOTAL_TESTS</p>
        <p class="pass">Passed: $PASSED_TESTS</p>
        <p class="fail">Failed: $FAILED_TESTS</p>
        <p>Success Rate: $(( PASSED_TESTS * 100 / TOTAL_TESTS ))%</p>
    </div>
    
    <h2>Test Details</h2>
EOF

    # Add details for each test
    for test_file in "$OUTPUT_DIR"/*.json; do
        if [ -f "$test_file" ]; then
            local test_name=$(basename "$test_file" .json)
            local diff_file="$OUTPUT_DIR/${test_name}.diff"
            
            echo "    <h3>$test_name</h3>" >> "$report_file"
            
            if [ -f "$diff_file" ]; then
                echo "    <p class=\"fail\">❌ FAILED</p>" >> "$report_file"
                echo "    <h4>Differences:</h4>" >> "$report_file"
                echo "    <pre>$(cat "$diff_file")</pre>" >> "$report_file"
            else
                echo "    <p class=\"pass\">✅ PASSED</p>" >> "$report_file"
            fi
            
            echo "    <h4>Actual Output:</h4>" >> "$report_file"
            echo "    <pre>$(cat "$test_file")</pre>" >> "$report_file"
        fi
    done
    
    echo "</body></html>" >> "$report_file"
    echo -e "\n${CYAN}Test report generated: $report_file${NC}"
}

# Main execution
main() {
    echo -e "${BLUE}Starting FauxDB Test Runner...${NC}"
    
    setup_directories
    setup_postgresql
    check_server
    generate_tests
    run_all_tests
    
    echo -e "\n${BLUE}=== Test Summary ===${NC}"
    echo "Total Tests: $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
    echo "Success Rate: $(( PASSED_TESTS * 100 / TOTAL_TESTS ))%"
    
    generate_report
    
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "\n${GREEN}🎉 All tests passed!${NC}"
        exit 0
    else
        echo -e "\n${YELLOW}⚠️  Some tests failed. Check output directory for details.${NC}"
        echo "Output directory: $OUTPUT_DIR"
        exit 1
    fi
}

# Handle script arguments
case "${1:-}" in
    "setup")
        setup_directories
        setup_postgresql
        echo "Setup completed"
        ;;
    "generate")
        setup_directories
        generate_tests
        echo "Test cases generated"
        ;;
    "check")
        check_server
        ;;
    *)
        main
        ;;
esac
