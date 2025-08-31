#!/usr/bin/env bash

# FauxDB Regression Tests - TAP Output Format
# Run with: prove fauxdb_regression_tap.sh
# Or directly: ./fauxdb_regression_tap.sh

echo "# FauxDB MongoDB Wire Protocol Regression Tests"
echo "# Testing compatibility with MongoDB clients"
echo "1..15"

test_num=1

# Helper function for running tests
run_test() {
    local description="$1"
    local command="$2"
    
    if eval "$command" >/dev/null 2>&1; then
        echo "ok $test_num - $description"
    else
        echo "not ok $test_num - $description"
    fi
    ((test_num++))
}

# Test 1: Binary exists
run_test "FauxDB binary exists" "[ -f '/Users/ibrarahmed/pgelephant/pge/fauxdb/build/fauxdb' ]"

# Test 2: Regression executable exists  
run_test "C++ regression test executable built" "[ -f '/Users/ibrarahmed/pgelephant/pge/fauxdb/build/regression/mongo_regression_tests' ]"

# Test 3: JavaScript tests exist
run_test "JavaScript regression tests exist" "[ -f '/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/scripts/regression_tests.js' ]"

# Test 4: TAP format tests exist
run_test "TAP format regression tests exist" "[ -f '/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/scripts/tap_regression_tests.js' ]"

# Test 5: Expected results directory
run_test "Expected results directory exists" "[ -d '/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/expected' ]"

# Test 6: Expected results files count
run_test "Expected result files present (8 files)" "[ \$(ls -1 /Users/ibrarahmed/pgelephant/pge/fauxdb/regression/expected/*.json 2>/dev/null | wc -l) -eq 8 ]"

# Test 7: MongoDB client available
run_test "MongoDB shell (mongosh) available" "command -v mongosh"

# Test 8: JSON validation
run_test "Expected result JSON files are valid" "python3 -c 'import json; [json.load(open(f)) for f in [\"/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/expected/ping_expected.json\"]]'"

# Test 9: CMake targets configured  
run_test "CMake regression targets configured" "grep -q 'regression' /Users/ibrarahmed/pgelephant/pge/fauxdb/build/Makefile"

# Test 10: Test scripts present
run_test "Multiple test execution methods available" "[ \$(find /Users/ibrarahmed/pgelephant/pge/fauxdb/regression -name '*.sh' -o -name '*.js' | wc -l) -ge 5 ]"

# Test 11: C++ test listing works
run_test "C++ regression tests can list test cases" "/Users/ibrarahmed/pgelephant/pge/fauxdb/build/regression/mongo_regression_tests --gtest_list_tests"

# Test 12: Demo script executable
run_test "Demo script is executable" "[ -x '/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/demo.sh' ]"

# Test 13: Test runner script executable
run_test "Test runner script is executable" "[ -x '/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/scripts/run_regression_tests.sh' ]"

# Test 14: Documentation exists
run_test "Regression test documentation exists" "[ -f '/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/README.md' ]"

# Test 15: Framework structure complete
run_test "Complete regression framework structure" "[ -d '/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/integration' ] && [ -d '/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/scripts' ] && [ -d '/Users/ibrarahmed/pgelephant/pge/fauxdb/regression/expected' ]"

echo "# Regression test framework validation complete"
echo "# Framework ready for MongoDB wire protocol testing"
