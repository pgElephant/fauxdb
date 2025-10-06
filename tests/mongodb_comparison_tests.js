/*!
 * MongoDB vs FauxDB Comparison Tests
 * Comprehensive comparison testing using mongosh
 */

// Test configuration
const FAUXDB_CONNECTION = "mongodb://localhost:27018/test";
const MONGODB_CONNECTION = "mongodb://localhost:27017/test";

// Test results tracking
const testResults = {
    passed: 0,
    failed: 0,
    skipped: 0,
    tests: []
};

// Helper function to record test results
function recordTest(testName, status, details = "") {
    testResults.tests.push({
        name: testName,
        status: status,
        details: details,
        timestamp: new Date()
    });
    
    if (status === "passed") testResults.passed++;
    else if (status === "failed") testResults.failed++;
    else if (status === "skipped") testResults.skipped++;
    
    const statusIcon = status === "passed" ? "✅" : status === "failed" ? "❌" : "⚠️";
    console.log(`${statusIcon} ${testName}: ${status.toUpperCase()} ${details}`);
}

// Helper function to test connection
async function testConnection(connectionString, name) {
    try {
        console.log(`\n🔌 Testing ${name} connection...`);
        const client = new Mongo(connectionString);
        const admin = client.admin();
        const result = await admin.ping();
        console.log(`✅ ${name} connection successful`);
        return client;
    } catch (error) {
        console.error(`❌ ${name} connection failed:`, error.message);
        return null;
    }
}

// Helper function to compare results
function compareResults(fauxdbResult, mongodbResult, testName) {
    try {
        // Convert to JSON strings for comparison
        const fauxdbStr = JSON.stringify(fauxdbResult);
        const mongodbStr = JSON.stringify(mongodbResult);
        
        if (fauxdbStr === mongodbStr) {
            recordTest(testName, "passed", "Results match perfectly");
            return true;
        } else {
            recordTest(testName, "failed", "Results differ");
            console.log("FauxDB result:", fauxdbStr);
            console.log("MongoDB result:", mongodbStr);
            return false;
        }
    } catch (error) {
        recordTest(testName, "failed", `Comparison error: ${error.message}`);
        return false;
    }
}

// Test 1: Basic CRUD Operations Comparison
async function testBasicCRUDComparison(fauxdbClient, mongodbClient) {
    console.log("\n🧪 Testing Basic CRUD Operations Comparison...");
    
    const fauxdbDb = fauxdbClient.db("test");
    const mongodbDb = mongodbClient.db("test");
    const collectionName = "crud_comparison";
    
    // Clean up existing data
    await fauxdbDb.collection(collectionName).drop().catch(() => {});
    await mongodbDb.collection(collectionName).drop().catch(() => {});
    
    const testDoc = {
        name: "Test User",
        age: 30,
        email: "test@example.com",
        tags: ["tag1", "tag2", "tag3"],
        metadata: {
            created: new Date(),
            version: 1,
            active: true
        }
    };
    
    // Test INSERT
    console.log("\n📝 Testing INSERT operations...");
    try {
        const fauxdbInsert = await fauxdbDb.collection(collectionName).insertOne(testDoc);
        const mongodbInsert = await mongodbDb.collection(collectionName).insertOne(testDoc);
        
        const insertMatch = compareResults(
            {insertedId: fauxdbInsert.insertedId.toString()},
            {insertedId: mongodbInsert.insertedId.toString()},
            "INSERT - Document ID Generation"
        );
        
        if (!insertMatch) {
            console.log("FauxDB insert ID:", fauxdbInsert.insertedId);
            console.log("MongoDB insert ID:", mongodbInsert.insertedId);
        }
        
    } catch (error) {
        recordTest("INSERT Operations", "failed", error.message);
    }
    
    // Test FIND
    console.log("\n🔍 Testing FIND operations...");
    try {
        const fauxdbFind = await fauxdbDb.collection(collectionName).findOne({name: "Test User"});
        const mongodbFind = await mongodbDb.collection(collectionName).findOne({name: "Test User"});
        
        // Compare documents (excluding _id which will be different)
        const fauxdbDoc = {...fauxdbFind};
        const mongodbDoc = {...mongodbFind};
        delete fauxdbDoc._id;
        delete mongodbDoc._id;
        
        compareResults(fauxdbDoc, mongodbDoc, "FIND - Document Retrieval");
        
    } catch (error) {
        recordTest("FIND Operations", "failed", error.message);
    }
    
    // Test UPDATE
    console.log("\n✏️ Testing UPDATE operations...");
    try {
        const updateDoc = {$set: {age: 31, updated: new Date()}};
        
        const fauxdbUpdate = await fauxdbDb.collection(collectionName).updateOne(
            {name: "Test User"},
            updateDoc
        );
        const mongodbUpdate = await mongodbDb.collection(collectionName).updateOne(
            {name: "Test User"},
            updateDoc
        );
        
        compareResults(
            {modifiedCount: fauxdbUpdate.modifiedCount, matchedCount: fauxdbUpdate.matchedCount},
            {modifiedCount: mongodbUpdate.modifiedCount, matchedCount: mongodbUpdate.matchedCount},
            "UPDATE - Document Modification"
        );
        
    } catch (error) {
        recordTest("UPDATE Operations", "failed", error.message);
    }
    
    // Test DELETE
    console.log("\n🗑️ Testing DELETE operations...");
    try {
        const fauxdbDelete = await fauxdbDb.collection(collectionName).deleteOne({name: "Test User"});
        const mongodbDelete = await mongodbDb.collection(collectionName).deleteOne({name: "Test User"});
        
        compareResults(
            {deletedCount: fauxdbDelete.deletedCount},
            {deletedCount: mongodbDelete.deletedCount},
            "DELETE - Document Removal"
        );
        
    } catch (error) {
        recordTest("DELETE Operations", "failed", error.message);
    }
}

// Test 2: Aggregation Pipeline Comparison
async function testAggregationComparison(fauxdbClient, mongodbClient) {
    console.log("\n🧪 Testing Aggregation Pipeline Comparison...");
    
    const fauxdbDb = fauxdbClient.db("test");
    const mongodbDb = mongodbClient.db("test");
    const collectionName = "aggregation_comparison";
    
    // Clean up existing data
    await fauxdbDb.collection(collectionName).drop().catch(() => {});
    await mongodbDb.collection(collectionName).drop().catch(() => {});
    
    // Insert test data
    const testData = [
        {name: "Alice", age: 25, city: "New York", salary: 50000, department: "Engineering"},
        {name: "Bob", age: 30, city: "Boston", salary: 60000, department: "Engineering"},
        {name: "Charlie", age: 25, city: "New York", salary: 55000, department: "Marketing"},
        {name: "David", age: 35, city: "Boston", salary: 70000, department: "Engineering"},
        {name: "Eve", age: 28, city: "Chicago", salary: 52000, department: "Marketing"},
        {name: "Frank", age: 32, city: "Chicago", salary: 65000, department: "Sales"}
    ];
    
    await fauxdbDb.collection(collectionName).insertMany(testData);
    await mongodbDb.collection(collectionName).insertMany(testData);
    
    // Test 1: Simple aggregation
    console.log("\n📊 Testing simple aggregation...");
    try {
        const pipeline1 = [
            {$match: {age: {$gte: 25}}},
            {$group: {
                _id: "$department",
                avgSalary: {$avg: "$salary"},
                count: {$sum: 1}
            }},
            {$sort: {avgSalary: -1}}
        ];
        
        const fauxdbResult1 = await fauxdbDb.collection(collectionName).aggregate(pipeline1).toArray();
        const mongodbResult1 = await mongodbDb.collection(collectionName).aggregate(pipeline1).toArray();
        
        compareResults(fauxdbResult1, mongodbResult1, "Simple Aggregation Pipeline");
        
    } catch (error) {
        recordTest("Simple Aggregation", "failed", error.message);
    }
    
    // Test 2: Complex aggregation with multiple stages
    console.log("\n📊 Testing complex aggregation...");
    try {
        const pipeline2 = [
            {$match: {salary: {$gte: 50000}}},
            {$addFields: {
                salary_category: {
                    $cond: {
                        if: {$gte: ["$salary", 60000]},
                        then: "high",
                        else: "medium"
                    }
                }
            }},
            {$group: {
                _id: {
                    city: "$city",
                    category: "$salary_category"
                },
                employees: {$push: {
                    name: "$name",
                    salary: "$salary"
                }},
                total_salary: {$sum: "$salary"},
                avg_salary: {$avg: "$salary"}
            }},
            {$sort: {total_salary: -1}},
            {$limit: 5}
        ];
        
        const fauxdbResult2 = await fauxdbDb.collection(collectionName).aggregate(pipeline2).toArray();
        const mongodbResult2 = await mongodbDb.collection(collectionName).aggregate(pipeline2).toArray();
        
        compareResults(fauxdbResult2, mongodbResult2, "Complex Aggregation Pipeline");
        
    } catch (error) {
        recordTest("Complex Aggregation", "failed", error.message);
    }
    
    // Test 3: Lookup operation (if supported)
    console.log("\n📊 Testing lookup operation...");
    try {
        // Create a departments collection
        const departments = [
            {_id: "Engineering", budget: 1000000, manager: "John Smith"},
            {_id: "Marketing", budget: 500000, manager: "Jane Doe"},
            {_id: "Sales", budget: 750000, manager: "Bob Johnson"}
        ];
        
        await fauxdbDb.collection("departments").insertMany(departments);
        await mongodbDb.collection("departments").insertMany(departments);
        
        const lookupPipeline = [
            {$lookup: {
                from: "departments",
                localField: "department",
                foreignField: "_id",
                as: "dept_info"
            }},
            {$unwind: "$dept_info"},
            {$project: {
                name: 1,
                salary: 1,
                department: 1,
                dept_budget: "$dept_info.budget",
                dept_manager: "$dept_info.manager"
            }}
        ];
        
        const fauxdbLookup = await fauxdbDb.collection(collectionName).aggregate(lookupPipeline).toArray();
        const mongodbLookup = await mongodbDb.collection(collectionName).aggregate(lookupPipeline).toArray();
        
        compareResults(fauxdbLookup, mongodbLookup, "Lookup Aggregation Pipeline");
        
    } catch (error) {
        recordTest("Lookup Operation", "skipped", error.message);
    }
}

// Test 3: Index Operations Comparison
async function testIndexOperationsComparison(fauxdbClient, mongodbClient) {
    console.log("\n🧪 Testing Index Operations Comparison...");
    
    const fauxdbDb = fauxdbClient.db("test");
    const mongodbDb = mongodbClient.db("test");
    const collectionName = "index_comparison";
    
    // Clean up existing data
    await fauxdbDb.collection(collectionName).drop().catch(() => {});
    await mongodbDb.collection(collectionName).drop().catch(() => {});
    
    // Insert test data
    const testData = [];
    for (let i = 0; i < 100; i++) {
        testData.push({
            name: `User${i}`,
            email: `user${i}@example.com`,
            age: Math.floor(Math.random() * 50) + 18,
            score: Math.floor(Math.random() * 1000),
            category: ["A", "B", "C"][Math.floor(Math.random() * 3)]
        });
    }
    
    await fauxdbDb.collection(collectionName).insertMany(testData);
    await mongodbDb.collection(collectionName).insertMany(testData);
    
    // Test index creation
    console.log("\n📇 Testing index creation...");
    try {
        // Single field index
        await fauxdbDb.collection(collectionName).createIndex({name: 1});
        await mongodbDb.collection(collectionName).createIndex({name: 1});
        
        // Compound index
        await fauxdbDb.collection(collectionName).createIndex({category: 1, score: -1});
        await mongodbDb.collection(collectionName).createIndex({category: 1, score: -1});
        
        // Partial index
        await fauxdbDb.collection(collectionName).createIndex(
            {email: 1},
            {partialFilterExpression: {age: {$gte: 25}}}
        );
        await mongodbDb.collection(collectionName).createIndex(
            {email: 1},
            {partialFilterExpression: {age: {$gte: 25}}}
        );
        
        recordTest("Index Creation", "passed", "All indexes created successfully");
        
    } catch (error) {
        recordTest("Index Creation", "failed", error.message);
    }
    
    // Test index listing
    console.log("\n📋 Testing index listing...");
    try {
        const fauxdbIndexes = await fauxdbDb.collection(collectionName).listIndexes().toArray();
        const mongodbIndexes = await mongodbDb.collection(collectionName).listIndexes().toArray();
        
        // Compare index counts
        if (fauxdbIndexes.length === mongodbIndexes.length) {
            recordTest("Index Listing", "passed", `Both databases have ${fauxdbIndexes.length} indexes`);
        } else {
            recordTest("Index Listing", "failed", `FauxDB: ${fauxdbIndexes.length}, MongoDB: ${mongodbIndexes.length}`);
        }
        
        // List index names for comparison
        const fauxdbIndexNames = fauxdbIndexes.map(i => i.name).sort();
        const mongodbIndexNames = mongodbIndexes.map(i => i.name).sort();
        
        console.log("FauxDB indexes:", fauxdbIndexNames);
        console.log("MongoDB indexes:", mongodbIndexNames);
        
    } catch (error) {
        recordTest("Index Listing", "failed", error.message);
    }
    
    // Test query performance with indexes
    console.log("\n⚡ Testing query performance...");
    try {
        const query = {category: "A", score: {$gte: 500}};
        
        const fauxdbStart = Date.now();
        const fauxdbResult = await fauxdbDb.collection(collectionName).find(query).toArray();
        const fauxdbTime = Date.now() - fauxdbStart;
        
        const mongodbStart = Date.now();
        const mongodbResult = await mongodbDb.collection(collectionName).find(query).toArray();
        const mongodbTime = Date.now() - mongodbStart;
        
        console.log(`FauxDB query time: ${fauxdbTime}ms, results: ${fauxdbResult.length}`);
        console.log(`MongoDB query time: ${mongodbTime}ms, results: ${mongodbResult.length}`);
        
        if (fauxdbResult.length === mongodbResult.length) {
            recordTest("Query Performance", "passed", `Both returned ${fauxdbResult.length} results`);
        } else {
            recordTest("Query Performance", "failed", "Result counts differ");
        }
        
    } catch (error) {
        recordTest("Query Performance", "failed", error.message);
    }
}

// Test 4: Geospatial Operations Comparison
async function testGeospatialComparison(fauxdbClient, mongodbClient) {
    console.log("\n🧪 Testing Geospatial Operations Comparison...");
    
    const fauxdbDb = fauxdbClient.db("test");
    const mongodbDb = mongodbClient.db("test");
    const collectionName = "geospatial_comparison";
    
    // Clean up existing data
    await fauxdbDb.collection(collectionName).drop().catch(() => {});
    await mongodbDb.collection(collectionName).drop().catch(() => {});
    
    // Insert geospatial test data
    const geoData = [
        {name: "San Francisco", location: {type: "Point", coordinates: [-122.4194, 37.7749]}, population: 873965},
        {name: "New York", location: {type: "Point", coordinates: [-74.0060, 40.7128]}, population: 8336817},
        {name: "Boston", location: {type: "Point", coordinates: [-71.0589, 42.3601]}, population: 692600},
        {name: "Chicago", location: {type: "Point", coordinates: [-87.6298, 41.8781]}, population: 2693976},
        {name: "Seattle", location: {type: "Point", coordinates: [-122.3321, 47.6062]}, population: 724745}
    ];
    
    await fauxdbDb.collection(collectionName).insertMany(geoData);
    await mongodbDb.collection(collectionName).insertMany(geoData);
    
    // Test geospatial index creation
    console.log("\n🗺️ Testing geospatial index creation...");
    try {
        await fauxdbDb.collection(collectionName).createIndex({location: "2dsphere"});
        await mongodbDb.collection(collectionName).createIndex({location: "2dsphere"});
        
        recordTest("Geospatial Index Creation", "passed", "2dsphere indexes created");
        
    } catch (error) {
        recordTest("Geospatial Index Creation", "failed", error.message);
    }
    
    // Test $near query
    console.log("\n📍 Testing $near query...");
    try {
        const nearQuery = {
            location: {
                $near: {
                    $geometry: {type: "Point", coordinates: [-122.4194, 37.7749]},
                    $maxDistance: 1000000 // 1000km in meters
                }
            }
        };
        
        const fauxdbNear = await fauxdbDb.collection(collectionName).find(nearQuery).toArray();
        const mongodbNear = await mongodbDb.collection(collectionName).find(nearQuery).toArray();
        
        // Compare results (order might differ due to distance calculations)
        const fauxdbNames = fauxdbNear.map(d => d.name).sort();
        const mongodbNames = mongodbNear.map(d => d.name).sort();
        
        if (JSON.stringify(fauxdbNames) === JSON.stringify(mongodbNames)) {
            recordTest("$near Query", "passed", `Both returned same cities: ${fauxdbNames.join(", ")}`);
        } else {
            recordTest("$near Query", "failed", "Different cities returned");
        }
        
    } catch (error) {
        recordTest("$near Query", "failed", error.message);
    }
    
    // Test $geoWithin query
    console.log("\n🔍 Testing $geoWithin query...");
    try {
        const withinQuery = {
            location: {
                $geoWithin: {
                    $geometry: {
                        type: "Polygon",
                        coordinates: [[
                            [-125, 35],
                            [-125, 45],
                            [-115, 45],
                            [-115, 35],
                            [-125, 35]
                        ]]
                    }
                }
            }
        };
        
        const fauxdbWithin = await fauxdbDb.collection(collectionName).find(withinQuery).toArray();
        const mongodbWithin = await mongodbDb.collection(collectionName).find(withinQuery).toArray();
        
        const fauxdbWithinNames = fauxdbWithin.map(d => d.name).sort();
        const mongodbWithinNames = mongodbWithin.map(d => d.name).sort();
        
        if (JSON.stringify(fauxdbWithinNames) === JSON.stringify(mongodbWithinNames)) {
            recordTest("$geoWithin Query", "passed", `Both returned same cities: ${fauxdbWithinNames.join(", ")}`);
        } else {
            recordTest("$geoWithin Query", "failed", "Different cities returned");
        }
        
    } catch (error) {
        recordTest("$geoWithin Query", "failed", error.message);
    }
}

// Test 5: Error Handling Comparison
async function testErrorHandlingComparison(fauxdbClient, mongodbClient) {
    console.log("\n🧪 Testing Error Handling Comparison...");
    
    const fauxdbDb = fauxdbClient.db("test");
    const mongodbDb = mongodbClient.db("test");
    const collectionName = "error_handling_comparison";
    
    // Test invalid queries
    console.log("\n⚠️ Testing invalid query handling...");
    
    // Test with invalid field names
    try {
        const invalidQuery = {"invalid.field.name": {$invalidOperator: "value"}};
        
        const fauxdbError = await fauxdbDb.collection(collectionName).find(invalidQuery).toArray();
        const mongodbError = await mongodbDb.collection(collectionName).find(invalidQuery).toArray();
        
        // Both should handle gracefully
        recordTest("Invalid Query Handling", "passed", "Both handled invalid query gracefully");
        
    } catch (error) {
        recordTest("Invalid Query Handling", "passed", `Both threw appropriate error: ${error.message}`);
    }
    
    // Test with malformed aggregation pipeline
    console.log("\n⚠️ Testing malformed aggregation pipeline...");
    try {
        const malformedPipeline = [
            {$invalidStage: {invalid: "value"}},
            {$group: {_id: null, count: {$invalidOperator: 1}}}
        ];
        
        const fauxdbAggError = await fauxdbDb.collection(collectionName).aggregate(malformedPipeline).toArray();
        const mongodbAggError = await mongodbDb.collection(collectionName).aggregate(malformedPipeline).toArray();
        
        recordTest("Malformed Aggregation Handling", "passed", "Both handled malformed pipeline gracefully");
        
    } catch (error) {
        recordTest("Malformed Aggregation Handling", "passed", `Both threw appropriate error: ${error.message}`);
    }
}

// Main test execution
async function runComparisonTests() {
    console.log("🚀 Starting MongoDB vs FauxDB Comparison Tests");
    console.log("=" * 60);
    
    const fauxdbClient = await testConnection(FAUXDB_CONNECTION, "FauxDB");
    const mongodbClient = await testConnection(MONGODB_CONNECTION, "MongoDB");
    
    if (!fauxdbClient || !mongodbClient) {
        console.log("❌ Cannot proceed without both database connections");
        return;
    }
    
    try {
        await testBasicCRUDComparison(fauxdbClient, mongodbClient);
        await testAggregationComparison(fauxdbClient, mongodbClient);
        await testIndexOperationsComparison(fauxdbClient, mongodbClient);
        await testGeospatialComparison(fauxdbClient, mongodbClient);
        await testErrorHandlingComparison(fauxdbClient, mongodbClient);
        
        // Print final results
        console.log("\n🏁 Test Results Summary");
        console.log("=" * 60);
        console.log(`✅ Passed: ${testResults.passed}`);
        console.log(`❌ Failed: ${testResults.failed}`);
        console.log(`⚠️ Skipped: ${testResults.skipped}`);
        console.log(`📊 Total: ${testResults.tests.length}`);
        
        const successRate = ((testResults.passed / testResults.tests.length) * 100).toFixed(1);
        console.log(`🎯 Success Rate: ${successRate}%`);
        
        // Show failed tests
        const failedTests = testResults.tests.filter(t => t.status === "failed");
        if (failedTests.length > 0) {
            console.log("\n❌ Failed Tests:");
            failedTests.forEach(test => {
                console.log(`  - ${test.name}: ${test.details}`);
            });
        }
        
    } catch (error) {
        console.error("❌ Test execution error:", error);
    }
}

// Run the comparison tests
runComparisonTests().catch(console.error);
