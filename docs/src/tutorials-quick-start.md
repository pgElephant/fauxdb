---
layout: default
title: Quick Start Tutorial
parent: Getting Started
nav_order: 2
description: Get up and running with FauxDB in minutes
---

# Quick Start Tutorial

This tutorial will get you up and running with FauxDB in just a few minutes. You'll learn how to install, configure, and use FauxDB as a MongoDB-compatible database.

## Prerequisites

- Basic knowledge of MongoDB operations
- Command line access
- Git installed on your system

## Step 1: Installation

### Option A: Automated Installation (Recommended)

```bash
# Clone the repository
git clone https://github.com/pgelephant/fauxdb.git
cd fauxdb

# Run the automated build script
./build.sh
```

The build script will:
- Install all system dependencies
- Set up PostgreSQL with proper configuration
- Build FauxDB with optimizations
- Create utility scripts
- Set up a MongoDB test container

### Option B: Manual Installation

If you prefer manual control:

```bash
# Install dependencies (Rocky Linux/RHEL/CentOS)
sudo dnf install -y postgresql postgresql-server postgresql-devel git

# Setup PostgreSQL
sudo postgresql-setup --initdb
sudo systemctl start postgresql
sudo systemctl enable postgresql

# Create database
sudo -u postgres psql -c "CREATE DATABASE fauxdb;"
sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE fauxdb TO postgres;"

# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source ~/.cargo/env

# Build FauxDB
cargo build --release
```

## Step 2: Start FauxDB

```bash
# Start the server
./run.sh

# Or manually
./target/release/fauxdb --config config/fauxdb.toml
```

You should see output like:
```
✓ FauxDB [Production FauxDB Server Starting]
✓ FauxDB [MongoDB 5.0+ Compatibility: ENABLED]
✓ FauxDB [Server Address: 0.0.0.0:27018]
✓ FauxDB [Database: postgresql://localhost/fauxdb]
```

## Step 3: Connect with MongoDB Shell

Open a new terminal and connect using `mongosh`:

```bash
# Connect to FauxDB (port 27018)
mongosh mongodb://localhost:27018

# Or if using the test container
mongosh mongodb://localhost:27017  # MongoDB test container
mongosh mongodb://localhost:27018  # FauxDB server
```

## Step 4: Basic Operations

Let's perform some basic MongoDB operations to verify everything works:

### Create a Database and Collection

```javascript
// Switch to a test database
use testdb

// Insert a document (creates collection automatically)
db.users.insertOne({
    name: "John Doe",
    email: "john@example.com",
    age: 30,
    address: {
        street: "123 Main St",
        city: "New York",
        zipCode: "10001"
    },
    tags: ["developer", "javascript"],
    createdAt: new Date()
})
```

### Query Documents

```javascript
// Find all users
db.users.find()

// Find with filter
db.users.find({age: {$gte: 25}})

// Find with projection
db.users.find(
    {age: {$gte: 25}},
    {name: 1, email: 1, _id: 0}
)

// Find one document
db.users.findOne({email: "john@example.com"})
```

### Update Documents

```javascript
// Update a single document
db.users.updateOne(
    {email: "john@example.com"},
    {$set: {age: 31}}
)

// Update multiple documents
db.users.updateMany(
    {age: {$lt: 25}},
    {$set: {status: "young"}}
)

// Replace a document
db.users.replaceOne(
    {email: "john@example.com"},
    {
        name: "John Smith",
        email: "john.smith@example.com",
        age: 32,
        status: "active"
    }
)
```

### Delete Documents

```javascript
// Delete a single document
db.users.deleteOne({email: "john@example.com"})

// Delete multiple documents
db.users.deleteMany({status: "inactive"})
```

## Step 5: Advanced Operations

### Aggregation Pipeline

```javascript
// Create some sample data
db.orders.insertMany([
    {customer: "Alice", amount: 100, date: new Date("2024-01-15")},
    {customer: "Bob", amount: 200, date: new Date("2024-01-16")},
    {customer: "Alice", amount: 150, date: new Date("2024-01-17")},
    {customer: "Charlie", amount: 300, date: new Date("2024-01-18")}
])

// Aggregation pipeline
db.orders.aggregate([
    // Match orders from January 2024
    {$match: {date: {$gte: new Date("2024-01-01")}}},
    
    // Group by customer
    {$group: {
        _id: "$customer",
        totalAmount: {$sum: "$amount"},
        orderCount: {$sum: 1},
        avgAmount: {$avg: "$amount"}
    }},
    
    // Sort by total amount
    {$sort: {totalAmount: -1}},
    
    // Add computed field
    {$addFields: {
        customerTier: {
            $switch: {
                branches: [
                    {case: {$gte: ["$totalAmount", 300]}, then: "Gold"},
                    {case: {$gte: ["$totalAmount", 200]}, then: "Silver"},
                    {case: {$gte: ["$totalAmount", 100]}, then: "Bronze"}
                ],
                default: "New"
            }
        }
    }}
])
```

### Indexing

```javascript
// Create indexes for better performance
db.users.createIndex({email: 1})  // Single field index
db.users.createIndex({age: 1, status: 1})  // Compound index
db.users.createIndex({address.city: 1})  // Embedded field index

// List indexes
db.users.getIndexes()

// Drop an index
db.users.dropIndex({email: 1})
```

### Text Search

```javascript
// Create text index
db.users.createIndex({
    name: "text",
    email: "text",
    tags: "text"
})

// Text search
db.users.find({$text: {$search: "developer javascript"}})
```

## Step 6: Verify PostgreSQL Integration

Let's verify that FauxDB is actually storing data in PostgreSQL:

```bash
# Connect to PostgreSQL
sudo -u postgres psql -d fauxdb

# List tables (collections become tables)
\dt

# Query data directly with SQL
SELECT * FROM testdb_users LIMIT 5;

# Query JSONB data
SELECT document->>'name' as name, 
       document->>'email' as email,
       document->'address'->>'city' as city
FROM testdb_users 
WHERE document->>'age' >= '25';
```

## Step 7: Test MongoDB Compatibility

Run the comprehensive test suite:

```bash
# Run automated tests
./test.sh

# Or run specific tests
cargo test --test fauxdb_tests test_crud_operations
cargo test --test fauxdb_tests test_aggregation_pipeline
```

## Step 8: Monitoring

Check FauxDB metrics and health:

```bash
# Health check
curl http://localhost:9090/health

# Metrics
curl http://localhost:9090/metrics

# Status
curl http://localhost:9090/status
```

## Step 9: Compare with MongoDB

Let's compare FauxDB with a real MongoDB instance:

```bash
# Connect to MongoDB test container
mongosh mongodb://localhost:27017

# Run the same operations
use testdb
db.users.insertOne({name: "Test", age: 25})
db.users.find()
```

```bash
# Connect to FauxDB
mongosh mongodb://localhost:27018

# Run the same operations
use testdb
db.users.insertOne({name: "Test", age: 25})
db.users.find()
```

Both should return identical results!

## Common Issues and Solutions

### Connection Refused

```bash
# Check if FauxDB is running
netstat -tlnp | grep 27018

# Check logs
tail -f /var/log/fauxdb.log
```

### PostgreSQL Connection Issues

```bash
# Check PostgreSQL status
sudo systemctl status postgresql

# Test connection
sudo -u postgres psql -d fauxdb
```

### Build Errors

```bash
# Clean and rebuild
cargo clean
cargo build --release

# Check Rust version
rustc --version
```

## Next Steps

Now that you have FauxDB running, explore these advanced topics:

- **Configuration**: Learn about [Configuration Options]({{ site.baseurl }}/guides/configuration/)
- **Performance**: Read about [Performance Tuning]({{ site.baseurl }}/guides/performance/)
- **Deployment**: Follow the [Production Deployment Guide]({{ site.baseurl }}/deployment/production/)
- **API Reference**: Explore the [Complete API Reference]({{ site.baseurl }}/api/operations/)

## Troubleshooting

### Logs and Debugging

```bash
# Enable debug logging
export RUST_LOG=debug
./target/release/fauxdb --config config/fauxdb.toml

# Check PostgreSQL logs
sudo tail -f /var/lib/pgsql/data/log/postgresql-*.log
```

### Performance Issues

```bash
# Check PostgreSQL performance
sudo -u postgres psql -d fauxdb -c "
SELECT query, calls, total_time, mean_time 
FROM pg_stat_statements 
ORDER BY total_time DESC 
LIMIT 10;
"
```

## Congratulations!

You've successfully set up FauxDB and verified MongoDB compatibility. You can now:

- Use FauxDB as a drop-in replacement for MongoDB
- Leverage PostgreSQL's ACID guarantees
- Query data with both MongoDB API and SQL
- Scale with PostgreSQL's proven architecture

Ready to dive deeper? Check out the [API Reference]({{ site.baseurl }}/api/operations/) for detailed operation documentation.
