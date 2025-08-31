// Document Database Regression Test Runner for FauxDB
// Usage: export CLIENT=mongosh && $CLIENT --port 27018 --host 127.0.0.1 regression_tests.js

print("=== FauxDB MongoDB Regression Tests ===");

// Test configuration
const TEST_DATABASE = "testdb";

// Test results tracking
let testsRun = 0;
let testsPassed = 0;
let testsFailed = 0;
const failedTests = [];

// Utility functions
function logTest(testName, status, message = "") {
    testsRun++;
    const statusSymbol = status === "PASS" ? "✓" : "✗";
    const statusColor = status === "PASS" ? "" : "";
    
    print(`${statusSymbol} ${testName}: ${status}`);
    if (message) {
        print(`   ${message}`);
    }
    
    if (status === "PASS") {
        testsPassed++;
    } else {
        testsFailed++;
        failedTests.push(testName);
    }
}

function compareResults(testName, actual, expected, description = "") {
    try {
        const actualStr = JSON.stringify(actual, null, 2);
        const expectedStr = JSON.stringify(expected, null, 2);
        
        // Normalize ObjectId comparisons (replace with placeholder)
        const normalizeIds = (str) => str.replace(/ObjectId\("[^"]+"\)/g, 'ObjectId("...")');
        const normalizedActual = normalizeIds(actualStr);
        const normalizedExpected = normalizeIds(expectedStr);
        
        if (normalizedActual === normalizedExpected) {
            logTest(testName, "PASS", description);
        } else {
            logTest(testName, "FAIL", `${description}\nExpected: ${normalizedExpected}\nActual: ${normalizedActual}`);
        }
    } catch (error) {
        logTest(testName, "FAIL", `Comparison error: ${error.message}`);
    }
}

// Test Cases

// Test 1: Basic ping command
function testPing() {
    try {
        const result = db.runCommand({ping: 1});
        const expected = {ok: 1};
        compareResults("ping", result, expected, "Basic server ping");
    } catch (error) {
        logTest("ping", "FAIL", `Error: ${error.message}`);
    }
}

// Test 2: List databases
function testListDatabases() {
    try {
        const result = db.adminCommand({listDatabases: 1});
        
        // Check if result has expected structure
        if (result.databases && Array.isArray(result.databases) && result.ok === 1) {
            logTest("list_databases", "PASS", "Database list returned successfully");
        } else {
            logTest("list_databases", "FAIL", `Unexpected result structure: ${JSON.stringify(result)}`);
        }
    } catch (error) {
        logTest("list_databases", "FAIL", `Error: ${error.message}`);
    }
}

// Test 3: Insert document
function testInsertDocument() {
    try {
        const result = db.users.insertOne({
            name: "John Doe",
            age: 30,
            email: "john@example.com",
            created: new Date()
        });
        
        if (result.acknowledged && result.insertedId) {
            logTest("insert_document", "PASS", "Document inserted successfully");
        } else {
            logTest("insert_document", "FAIL", `Insert failed: ${JSON.stringify(result)}`);
        }
    } catch (error) {
        logTest("insert_document", "FAIL", `Error: ${error.message}`);
    }
}

// Test 4: Find documents
function testFindDocuments() {
    try {
        // First insert some test data
        db.users.insertOne({name: "Jane Doe", age: 28, email: "jane@example.com"});
        db.users.insertOne({name: "Bob Smith", age: 35, email: "bob@example.com"});
        
        const result = db.users.find({age: {$gt: 25}}).toArray();
        
        if (Array.isArray(result) && result.length >= 2) {
            logTest("find_documents", "PASS", `Found ${result.length} documents`);
        } else {
            logTest("find_documents", "FAIL", `Expected at least 2 documents, got: ${result.length}`);
        }
    } catch (error) {
        logTest("find_documents", "FAIL", `Error: ${error.message}`);
    }
}

// Test 5: Update document
function testUpdateDocument() {
    try {
        // First insert test data
        const insertResult = db.users.insertOne({
            name: "Update Test",
            age: 35,
            status: "active"
        });
        
        // Now update it
        const updateResult = db.users.updateOne(
            {name: "Update Test"},
            {$set: {age: 36, updated: new Date()}}
        );
        
        if (updateResult.acknowledged && updateResult.modifiedCount === 1) {
            logTest("update_document", "PASS", "Document updated successfully");
        } else {
            logTest("update_document", "FAIL", `Update failed: ${JSON.stringify(updateResult)}`);
        }
    } catch (error) {
        logTest("update_document", "FAIL", `Error: ${error.message}`);
    }
}

// Test 6: Delete document
function testDeleteDocument() {
    try {
        // First insert test data
        db.users.insertOne({
            name: "Delete Test",
            temporary: true
        });
        
        // Now delete it
        const deleteResult = db.users.deleteOne({name: "Delete Test"});
        
        if (deleteResult.acknowledged && deleteResult.deletedCount === 1) {
            logTest("delete_document", "PASS", "Document deleted successfully");
        } else {
            logTest("delete_document", "FAIL", `Delete failed: ${JSON.stringify(deleteResult)}`);
        }
    } catch (error) {
        logTest("delete_document", "FAIL", `Error: ${error.message}`);
    }
}

// Test 7: Count documents
function testCountDocuments() {
    try {
        // Insert some test data
        db.test_collection.insertOne({category: "test", value: 1});
        db.test_collection.insertOne({category: "test", value: 2});
        db.test_collection.insertOne({category: "other", value: 3});
        
        const count = db.test_collection.countDocuments({category: "test"});
        
        if (count === 2) {
            logTest("count_documents", "PASS", "Count returned correctly");
        } else {
            logTest("count_documents", "FAIL", `Expected count 2, got: ${count}`);
        }
    } catch (error) {
        logTest("count_documents", "FAIL", `Error: ${error.message}`);
    }
}

// Test 8: Complex query with nested fields
function testComplexQuery() {
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
        
        db.employees.insertOne({
            name: "Bob",
            age: 25,
            skills: ["java", "python"],
            address: {
                city: "New York",
                state: "NY",
                zipcode: "10001"
            },
            salary: 65000
        });
        
        // Test complex query
        const result = db.employees.find({
            age: {$gte: 25, $lte: 35},
            "address.state": "CA",
            skills: {$in: ["python"]}
        }).toArray();
        
        if (Array.isArray(result) && result.length === 1 && result[0].name === "Alice") {
            logTest("complex_query", "PASS", "Complex query returned correct results");
        } else {
            logTest("complex_query", "FAIL", `Unexpected results: ${JSON.stringify(result)}`);
        }
    } catch (error) {
        logTest("complex_query", "FAIL", `Error: ${error.message}`);
    }
}

// Test 9: Index operations
function testIndexOperations() {
    try {
        // Create index
        const indexResult = db.users.createIndex({email: 1});
        
        // List indexes
        const indexes = db.users.getIndexes();
        
        if (Array.isArray(indexes) && indexes.length > 0) {
            logTest("index_operations", "PASS", `Created and listed indexes: ${indexes.length} indexes found`);
        } else {
            logTest("index_operations", "FAIL", "Index operations failed");
        }
    } catch (error) {
        logTest("index_operations", "FAIL", `Error: ${error.message}`);
    }
}

// Test 10: Aggregation pipeline
function testAggregation() {
    try {
        // Insert test data for aggregation
        db.sales.insertMany([
            {product: "laptop", price: 1000, quantity: 2, date: new Date("2023-01-01")},
            {product: "mouse", price: 25, quantity: 10, date: new Date("2023-01-02")},
            {product: "laptop", price: 1000, quantity: 1, date: new Date("2023-01-03")},
            {product: "keyboard", price: 50, quantity: 5, date: new Date("2023-01-04")}
        ]);
        
        // Test aggregation pipeline
        const result = db.sales.aggregate([
            {$group: {
                _id: "$product",
                totalRevenue: {$sum: {$multiply: ["$price", "$quantity"]}},
                totalQuantity: {$sum: "$quantity"}
            }},
            {$sort: {totalRevenue: -1}}
        ]).toArray();
        
        if (Array.isArray(result) && result.length > 0) {
            logTest("aggregation", "PASS", `Aggregation returned ${result.length} groups`);
        } else {
            logTest("aggregation", "FAIL", "Aggregation failed or returned no results");
        }
    } catch (error) {
        logTest("aggregation", "FAIL", `Error: ${error.message}`);
    }
}

// Test runner
function runAllTests() {
    print("\nStarting regression tests...\n");
    
    // Clean up collections before starting
    try {
        db.users.drop();
        db.test_collection.drop();
        db.employees.drop();
        db.sales.drop();
    } catch (e) {
        // Ignore errors if collections don't exist
    }
    
    // Run all tests
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
    
    // Print summary
    print("\n" + "=".repeat(50));
    print("           REGRESSION TEST SUMMARY");
    print("=".repeat(50));
    print(`Total Tests:     ${testsRun}`);
    print(`Passed:          ${testsPassed}`);
    print(`Failed:          ${testsFailed}`);
    print(`Success Rate:    ${((testsPassed / testsRun) * 100).toFixed(1)}%`);
    
    if (testsFailed > 0) {
        print("\nFailed Tests:");
        failedTests.forEach(test => print(`  - ${test}`));
    }
    
    print("=".repeat(50));
    
    if (testsFailed === 0) {
        print("All tests passed! FauxDB is working correctly.");
    } else {
        print(`${testsFailed} test(s) failed. Please check the implementation.`);
    }
}

// Use the correct database
use(TEST_DATABASE);

// Run the tests
runAllTests();
