// TAP-style Document Database Regression Test Runner for FauxDB
// Usage: export CLIENT=$CLIENT && $CLIENT --port 27018 --host 127.0.0.1 tap_regression_tests.js

// TAP (Test Anything Protocol) format output for prove compatibility

const TEST_DATABASE = "testdb";

// Test counter
let testCount = 0;
const tests = [];

// TAP helper functions
function plan(count) {
    print(`1..${count}`);
}

function ok(condition, description) {
    testCount++;
    if (condition) {
        print(`ok ${testCount} - ${description}`);
        return true;
    } else {
        print(`not ok ${testCount} - ${description}`);
        return false;
    }
}

function is(actual, expected, description) {
    testCount++;
    const actualStr = JSON.stringify(actual);
    const expectedStr = JSON.stringify(expected);
    
    if (actualStr === expectedStr) {
        print(`ok ${testCount} - ${description}`);
        return true;
    } else {
        print(`not ok ${testCount} - ${description}`);
        print(`# Expected: ${expectedStr}`);
        print(`# Got:      ${actualStr}`);
        return false;
    }
}

function like(actual, pattern, description) {
    testCount++;
    const actualStr = typeof actual === 'string' ? actual : JSON.stringify(actual);
    
    if (actualStr.match(pattern)) {
        print(`ok ${testCount} - ${description}`);
        return true;
    } else {
        print(`not ok ${testCount} - ${description}`);
        print(`# Pattern: ${pattern}`);
        print(`# Got:     ${actualStr}`);
        return false;
    }
}

function skip(reason, count = 1) {
    for (let i = 0; i < count; i++) {
        testCount++;
        print(`ok ${testCount} # SKIP ${reason}`);
    }
}

function todo(condition, description, reason) {
    testCount++;
    if (condition) {
        print(`ok ${testCount} - ${description} # TODO ${reason}`);
    } else {
        print(`not ok ${testCount} - ${description} # TODO ${reason}`);
    }
}

function diag(message) {
    print(`# ${message}`);
}

// Test functions
function testPing() {
    diag("Testing server ping command");
    try {
        const result = db.runCommand({ping: 1});
        ok(result.ok === 1, "ping command returns ok: 1");
        ok(typeof result.ok === 'number', "ping response has numeric ok field");
    } catch (error) {
        ok(false, `ping command failed: ${error.message}`);
    }
}

function testListDatabases() {
    diag("Testing database list command");
    try {
        const result = db.adminCommand({listDatabases: 1});
        ok(result.ok === 1, "listDatabases command succeeds");
        ok(Array.isArray(result.databases), "databases field is an array");
        ok(result.databases.length >= 0, "databases array is valid");
    } catch (error) {
        ok(false, `listDatabases command failed: ${error.message}`);
    }
}

function testInsertDocument() {
    diag("Testing document insertion");
    try {
        const result = db.users.insertOne({
            name: "John Doe",
            age: 30,
            email: "john@example.com",
            created: new Date()
        });
        
        ok(result.acknowledged === true, "insert operation acknowledged");
        ok(result.insertedId !== null, "insertedId is provided");
        like(result.insertedId.toString(), /^[a-f0-9]{24}$/, "insertedId looks like ObjectId");
    } catch (error) {
        ok(false, `insert operation failed: ${error.message}`);
    }
}

function testFindDocuments() {
    diag("Testing document queries");
    try {
        // Insert test data
        db.users.insertOne({name: "Jane Doe", age: 28, email: "jane@example.com"});
        db.users.insertOne({name: "Bob Smith", age: 35, email: "bob@example.com"});
        
        const result = db.users.find({age: {$gt: 25}}).toArray();
        
        ok(Array.isArray(result), "find returns an array");
        ok(result.length >= 2, "find returns expected number of documents");
        ok(result.every(doc => doc.age > 25), "all returned documents match filter");
    } catch (error) {
        ok(false, `find operation failed: ${error.message}`);
    }
}

function testUpdateDocument() {
    diag("Testing document updates");
    try {
        // Insert test data
        const insertResult = db.users.insertOne({
            name: "Update Test",
            age: 35,
            status: "active"
        });
        
        const updateResult = db.users.updateOne(
            {name: "Update Test"},
            {$set: {age: 36, updated: new Date()}}
        );
        
        ok(updateResult.acknowledged === true, "update operation acknowledged");
        ok(updateResult.matchedCount === 1, "update matched one document");
        ok(updateResult.modifiedCount === 1, "update modified one document");
    } catch (error) {
        ok(false, `update operation failed: ${error.message}`);
    }
}

function testDeleteDocument() {
    diag("Testing document deletion");
    try {
        // Insert test data
        db.users.insertOne({
            name: "Delete Test",
            temporary: true
        });
        
        const deleteResult = db.users.deleteOne({name: "Delete Test"});
        
        ok(deleteResult.acknowledged === true, "delete operation acknowledged");
        ok(deleteResult.deletedCount === 1, "delete removed one document");
    } catch (error) {
        ok(false, `delete operation failed: ${error.message}`);
    }
}

function testCountDocuments() {
    diag("Testing document counting");
    try {
        // Insert test data
        db.test_collection.insertOne({category: "test", value: 1});
        db.test_collection.insertOne({category: "test", value: 2});
        db.test_collection.insertOne({category: "other", value: 3});
        
        const count = db.test_collection.countDocuments({category: "test"});
        
        ok(typeof count === 'number', "count returns a number");
        ok(count === 2, "count returns correct number");
    } catch (error) {
        ok(false, `count operation failed: ${error.message}`);
    }
}

function testComplexQuery() {
    diag("Testing complex queries with nested fields");
    try {
        // Insert complex test data
        db.employees.insertOne({
            name: "Alice",
            age: 30,
            skills: ["python", "sql", "javascript"],
            address: {
                city: "San Francisco",
                state: "CA",
                zipcode: "94105"
            },
            salary: 75000
        });
        
        const result = db.employees.find({
            age: {$gte: 25, $lte: 35},
            "address.state": "CA",
            skills: {$in: ["python"]}
        }).toArray();
        
        ok(Array.isArray(result), "complex query returns array");
        ok(result.length === 1, "complex query returns correct count");
        ok(result[0].name === "Alice", "complex query returns correct document");
    } catch (error) {
        ok(false, `complex query failed: ${error.message}`);
    }
}

function testIndexOperations() {
    diag("Testing index operations");
    try {
        // Create index
        const indexResult = db.users.createIndex({email: 1});
        
        // List indexes
        const indexes = db.users.getIndexes();
        
        ok(Array.isArray(indexes), "getIndexes returns array");
        ok(indexes.length > 0, "at least one index exists");
        like(JSON.stringify(indexes), /"email"/, "email index was created");
    } catch (error) {
        ok(false, `index operations failed: ${error.message}`);
    }
}

function testAggregation() {
    diag("Testing aggregation pipeline");
    try {
        // Insert test data for aggregation
        db.sales.insertMany([
            {product: "laptop", price: 1000, quantity: 2, date: new Date("2023-01-01")},
            {product: "mouse", price: 25, quantity: 10, date: new Date("2023-01-02")},
            {product: "laptop", price: 1000, quantity: 1, date: new Date("2023-01-03")},
            {product: "keyboard", price: 50, quantity: 5, date: new Date("2023-01-04")}
        ]);
        
        const result = db.sales.aggregate([
            {$group: {
                _id: "$product",
                totalRevenue: {$sum: {$multiply: ["$price", "$quantity"]}},
                totalQuantity: {$sum: "$quantity"}
            }},
            {$sort: {totalRevenue: -1}}
        ]).toArray();
        
        ok(Array.isArray(result), "aggregation returns array");
        ok(result.length > 0, "aggregation returns results");
        ok(result.every(r => r._id && typeof r.totalRevenue === 'number'), "aggregation results have correct structure");
    } catch (error) {
        ok(false, `aggregation failed: ${error.message}`);
    }
}

// Main test runner
function runAllTests() {
    diag("TAP output for FauxDB MongoDB regression tests");
    diag("Using MongoDB shell to test wire protocol compatibility");
    
    // Clean up collections before starting
    try {
        db.users.drop();
        db.test_collection.drop();
        db.employees.drop();
        db.sales.drop();
    } catch (e) {
        diag("Collection cleanup completed (some may not have existed)");
    }
    
    // Plan for total number of tests
    plan(23); // Update this count based on actual test assertions
    
    // Run all test suites
    testPing();
    testListDatabases();
    testInsertDocument();
    testFindDocuments();
    testUpdateDocument();
    testDeleteDocument();
    testCountDocuments();
    testComplexQuery();
    testIndexOperations();
    testAggregation();
    
    diag("All tests completed");
}

// Use the correct database
use(TEST_DATABASE);

// Run the tests
runAllTests();
