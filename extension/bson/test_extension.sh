#!/bin/bash

# BSON Extension Test Script
# This script helps test the BSON extension after compilation and installation

echo "=== BSON Extension Test Script ==="
echo ""

# Check if PostgreSQL is running
echo "Checking PostgreSQL status..."
if pg_isready -h localhost -p 5432 > /dev/null 2>&1; then
    echo "✅ PostgreSQL is running on localhost:5432"
else
    echo "❌ PostgreSQL is not running on localhost:5432"
    echo "Please start PostgreSQL first:"
    echo "  brew services start postgresql@14"
    echo "  or"
    echo "  pg_ctl -D /opt/homebrew/var/postgresql@14 start"
    exit 1
fi

# Check if the extension is installed
echo ""
echo "Checking BSON extension installation..."
if [ -f "/opt/homebrew/lib/postgresql@14/bson.so" ]; then
    echo "✅ BSON extension library is installed"
else
    echo "❌ BSON extension library not found"
    exit 1
fi

if [ -f "/opt/homebrew/share/postgresql@14/extension/bson.control" ]; then
    echo "✅ BSON extension control file is installed"
else
    echo "❌ BSON extension control file not found"
    exit 1
fi

# Test the extension
echo ""
echo "Testing BSON extension..."
echo "Running test_extension.sql..."

# Create a test database
psql -h localhost -p 5432 -c "CREATE DATABASE test_bson;" 2>/dev/null || echo "Database test_bson already exists"

# Run the test script
if psql -h localhost -p 5432 -d test_bson -f test_extension.sql; then
    echo ""
    echo "✅ All tests passed successfully!"
    echo ""
    echo "BSON extension is working correctly!"
else
    echo ""
    echo "❌ Some tests failed. Check the output above for details."
    exit 1
fi

# Clean up test database
echo ""
echo "Cleaning up test database..."
psql -h localhost -p 5432 -c "DROP DATABASE IF EXISTS test_bson;"

echo ""
echo "=== Test completed ==="
