#!/bin/bash

# FauxDB Test Script
# Tests MongoDB compatibility through FauxDB

echo "Testing FauxDB MongoDB Compatibility..."
echo "=========================================="

# Test MongoDB shell connection to FauxDB
echo "Testing mongosh connection to FauxDB (port 27018)..."
podman exec mongodb-test mongosh --host localhost --port 27018 --eval "
    use test;
    db.users.insertOne({name: 'John', age: 30, email: 'john@example.com'});
    db.users.insertOne({name: 'Jane', age: 25, email: 'jane@example.com'});
    db.users.find().pretty();
    db.users.find({age: {\$gte: 30}}).pretty();
    db.users.countDocuments();
"

echo ""
echo "Testing aggregation pipeline..."
podman exec mongodb-test mongosh --host localhost --port 27018 --eval "
    use test;
    db.users.aggregate([
        {\$match: {age: {\$gte: 25}}},
        {\$group: {_id: null, avgAge: {\$avg: '\$age'}, count: {\$sum: 1}}}
    ]).pretty();
"

echo ""
echo "Test completed! Check the results above."
