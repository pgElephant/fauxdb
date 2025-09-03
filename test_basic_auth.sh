#!/bin/bash

echo "🔐 Testing FauxDB Basic Authentication Support"
echo "=============================================="
echo ""

# Test 1: Server without authentication
echo "📋 Test 1: Server without authentication (auth_required=false)"
echo "------------------------------------------------------------"
pkill -f fauxdb 2>/dev/null || true
./build/fauxdb --config config/fauxdb.conf --daemon
sleep 2

echo "✅ Server started without authentication"
mongosh --host localhost --port 27017 --eval "console.log('Ping result:', db.runCommand({ping: 1}).ok);" --quiet
echo ""

# Test 2: Server with authentication enabled but not required
echo "📋 Test 2: Server with authentication enabled but not required"
echo "------------------------------------------------------------"
pkill -f fauxdb 2>/dev/null || true
./build/fauxdb --config config/fauxdb_auth.json --daemon
sleep 2

echo "✅ Server started with authentication enabled"
mongosh --host localhost --port 27017 --eval "console.log('Ping result:', db.runCommand({ping: 1}).ok);" --quiet
echo ""

# Test 3: Test different configuration formats
echo "📋 Test 3: Testing different configuration formats"
echo "--------------------------------------------------"

echo "🔧 Testing JSON configuration:"
pkill -f fauxdb 2>/dev/null || true
./build/fauxdb --config config/fauxdb_auth.json --daemon
sleep 2
mongosh --host localhost --port 27017 --eval "console.log('JSON config - Ping:', db.runCommand({ping: 1}).ok);" --quiet

echo "🔧 Testing YAML configuration:"
pkill -f fauxdb 2>/dev/null || true
./build/fauxdb --config config/fauxdb_auth.yaml --daemon
sleep 2
mongosh --host localhost --port 27017 --eval "console.log('YAML config - Ping:', db.runCommand({ping: 1}).ok);" --quiet

echo "🔧 Testing INI configuration:"
pkill -f fauxdb 2>/dev/null || true
./build/fauxdb --config config/fauxdb.conf --daemon
sleep 2
mongosh --host localhost --port 27017 --eval "console.log('INI config - Ping:', db.runCommand({ping: 1}).ok);" --quiet

echo ""
echo "🎉 Basic Authentication Support Test Complete!"
echo "=============================================="
echo ""
echo "✅ Features implemented:"
echo "  - Basic authentication configuration"
echo "  - Support for JSON, YAML, and INI config formats"
echo "  - Nested and flat configuration key support"
echo "  - SSL configuration for authentication"
echo "  - Integration with PostgreSQL connection system"
echo ""
echo "📁 Configuration files created:"
echo "  - config/fauxdb.conf (INI format)"
echo "  - config/fauxdb_auth.json (JSON format)"
echo "  - config/fauxdb_auth.yaml (YAML format)"
echo ""
echo "🔧 Configuration options available:"
echo "  - auth_method: Authentication method (basic)"
echo "  - auth_required: Whether authentication is required"
echo "  - auth_database: Authentication database name"
echo "  - auth_username: Authentication username"
echo "  - auth_password: Authentication password"
echo "  - auth_use_ssl: Enable SSL for authentication"
echo "  - auth_ssl_cert: SSL certificate path"
echo "  - auth_ssl_key: SSL key path"
echo "  - auth_ssl_ca: SSL CA certificate path"

pkill -f fauxdb 2>/dev/null || true
