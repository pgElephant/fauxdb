/*!
 * MongoDB Shell (mongosh) Test Scripts for FauxDB
 * Tests FauxDB compatibility with MongoDB operations using mongosh
 */

// Test configuration
const FAUXDB_CONNECTION = "mongodb://localhost:27018/test";
const MONGODB_CONNECTION = "mongodb://localhost:27017/test";

// Helper function to test connection
async function testConnection(connectionString, name) {
    try {
        console.log(`\n🔌 Testing ${name} connection...`);
        const client = new Mongo(connectionString);
        const admin = client.admin();
        const result = await admin.ping();
        console.log(`✅ ${name} connection successful:`, result);
        return client;
    } catch (error) {
        console.error(`❌ ${name} connection failed:`, error.message);
        return null;
    }
}

// Helper function to run tests on both databases
async function runComparisonTest(testName, testFunction) {
    console.log(`\n🧪 Running test: ${testName}`);
    console.log("=" * 50);
    
    const fauxdbClient = await testConnection(FAUXDB_CONNECTION, "FauxDB");
    const mongodbClient = await testConnection(MONGODB_CONNECTION, "MongoDB");
    
    if (!fauxdbClient || !mongodbClient) {
        console.log("⚠️ Skipping test due to connection issues");
        return;
    }
    
    try {
        await testFunction(fauxdbClient, mongodbClient);
        console.log(`✅ ${testName} completed successfully`);
    } catch (error) {
        console.error(`❌ ${testName} failed:`, error.message);
    }
}

// Test 1: Basic CRUD Operations
async function testBasicCRUD(fauxdbClient, mongodbClient) {
    const fauxdbDb = fauxdbClient.db("test");
    const mongodbDb = mongodbClient.db("test");
    const collectionName = "crud_test";
    
    // Clean up existing data
    await fauxdbDb.collection(collectionName).drop().catch(() => {});
    await mongodbDb.collection(collectionName).drop().catch(() => {});
    
    const testDoc = {
        name: "John Doe",
        age: 30,
        email: "john@example.com",
        location: {
            type: "Point",
            coordinates: [-122.4194, 37.7749]
        },
        tags: ["developer", "mongodb", "postgresql"],
        created_at: new Date()
    };
    
    // Insert test
    console.log("\n📝 Testing INSERT operations...");
    const fauxdbInsert = await fauxdbDb.collection(collectionName).insertOne(testDoc);
    const mongodbInsert = await mongodbDb.collection(collectionName).insertOne(testDoc);
    console.log("FauxDB insert result:", fauxdbInsert.insertedId);
    console.log("MongoDB insert result:", mongodbInsert.insertedId);
    
    // Find test
    console.log("\n🔍 Testing FIND operations...");
    const fauxdbFind = await fauxdbDb.collection(collectionName).findOne({name: "John Doe"});
    const mongodbFind = await mongodbDb.collection(collectionName).findOne({name: "John Doe"});
    console.log("FauxDB find result:", fauxdbFind ? "Document found" : "Document not found");
    console.log("MongoDB find result:", mongodbFind ? "Document found" : "Document not found");
    
    // Update test
    console.log("\n✏️ Testing UPDATE operations...");
    const fauxdbUpdate = await fauxdbDb.collection(collectionName).updateOne(
        {name: "John Doe"},
        {$set: {age: 31, updated_at: new Date()}}
    );
    const mongodbUpdate = await mongodbDb.collection(collectionName).updateOne(
        {name: "John Doe"},
        {$set: {age: 31, updated_at: new Date()}}
    );
    console.log("FauxDB update result:", fauxdbUpdate.modifiedCount, "documents modified");
    console.log("MongoDB update result:", mongodbUpdate.modifiedCount, "documents modified");
    
    // Delete test
    console.log("\n🗑️ Testing DELETE operations...");
    const fauxdbDelete = await fauxdbDb.collection(collectionName).deleteOne({name: "John Doe"});
    const mongodbDelete = await mongodbDb.collection(collectionName).deleteOne({name: "John Doe"});
    console.log("FauxDB delete result:", fauxdbDelete.deletedCount, "documents deleted");
    console.log("MongoDB delete result:", mongodbDelete.deletedCount, "documents deleted");
}

// Test 2: Aggregation Pipeline
async function testAggregationPipeline(fauxdbClient, mongodbClient) {
    const fauxdbDb = fauxdbClient.db("test");
    const mongodbDb = mongodbClient.db("test");
    const collectionName = "aggregation_test";
    
    // Clean up existing data
    await fauxdbDb.collection(collectionName).drop().catch(() => {});
    await mongodbDb.collection(collectionName).drop().catch(() => {});
    
    // Insert test data
    const testData = [
        {name: "Alice", age: 25, city: "New York", salary: 50000},
        {name: "Bob", age: 30, city: "Boston", salary: 60000},
        {name: "Charlie", age: 25, city: "New York", salary: 55000},
        {name: "David", age: 35, city: "Boston", salary: 70000},
        {name: "Eve", age: 28, city: "Chicago", salary: 52000}
    ];
    
    await fauxdbDb.collection(collectionName).insertMany(testData);
    await mongodbDb.collection(collectionName).insertMany(testData);
    
    console.log("\n📊 Testing Aggregation Pipeline...");
    
    const pipeline = [
        {$match: {age: {$gte: 25}}},
        {$group: {
            _id: "$city",
            avgSalary: {$avg: "$salary"},
            count: {$sum: 1}
        }},
        {$sort: {avgSalary: -1}},
        {$limit: 3}
    ];
    
    // Test aggregation on both databases
    const fauxdbResult = await fauxdbDb.collection(collectionName).aggregate(pipeline).toArray();
    const mongodbResult = await mongodbDb.collection(collectionName).aggregate(pipeline).toArray();
    
    console.log("FauxDB aggregation result:", JSON.stringify(fauxdbResult, null, 2));
    console.log("MongoDB aggregation result:", JSON.stringify(mongodbResult, null, 2));
}

// Test 3: Geospatial Operations
async function testGeospatialOperations(fauxdbClient, mongodbClient) {
    const fauxdbDb = fauxdbClient.db("test");
    const mongodbDb = mongodbClient.db("test");
    const collectionName = "geospatial_test";
    
    // Clean up existing data
    await fauxdbDb.collection(collectionName).drop().catch(() => {});
    await mongodbDb.collection(collectionName).drop().catch(() => {});
    
    // Insert geospatial test data
    const geoData = [
        {name: "San Francisco", location: {type: "Point", coordinates: [-122.4194, 37.7749]}},
        {name: "New York", location: {type: "Point", coordinates: [-74.0060, 40.7128]}},
        {name: "Boston", location: {type: "Point", coordinates: [-71.0589, 42.3601]}},
        {name: "Chicago", location: {type: "Point", coordinates: [-87.6298, 41.8781]}},
        {name: "Seattle", location: {type: "Point", coordinates: [-122.3321, 47.6062]}}
    ];
    
    await fauxdbDb.collection(collectionName).insertMany(geoData);
    await mongodbDb.collection(collectionName).insertMany(geoData);
    
    console.log("\n🗺️ Testing Geospatial Operations...");
    
    // Create 2dsphere index
    try {
        await fauxdbDb.collection(collectionName).createIndex({location: "2dsphere"});
        console.log("✅ FauxDB 2dsphere index created");
    } catch (error) {
        console.log("⚠️ FauxDB index creation:", error.message);
    }
    
    try {
        await mongodbDb.collection(collectionName).createIndex({location: "2dsphere"});
        console.log("✅ MongoDB 2dsphere index created");
    } catch (error) {
        console.log("⚠️ MongoDB index creation:", error.message);
    }
    
    // Test $near query
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
    
    console.log("FauxDB $near results:", fauxdbNear.map(d => d.name));
    console.log("MongoDB $near results:", mongodbNear.map(d => d.name));
}

// Test 4: Index Operations
async function testIndexOperations(fauxdbClient, mongodbClient) {
    const fauxdbDb = fauxdbClient.db("test");
    const mongodbDb = mongodbClient.db("test");
    const collectionName = "index_test";
    
    console.log("\n📇 Testing Index Operations...");
    
    try {
        // Create indexes on both databases
        await fauxdbDb.collection(collectionName).createIndex({name: 1});
        await fauxdbDb.collection(collectionName).createIndex({age: -1});
        await fauxdbDb.collection(collectionName).createIndex({name: 1, age: 1});
        
        await mongodbDb.collection(collectionName).createIndex({name: 1});
        await mongodbDb.collection(collectionName).createIndex({age: -1});
        await mongodbDb.collection(collectionName).createIndex({name: 1, age: 1});
        
        console.log("✅ Indexes created successfully on both databases");
        
        // List indexes
        const fauxdbIndexes = await fauxdbDb.collection(collectionName).listIndexes().toArray();
        const mongodbIndexes = await mongodbDb.collection(collectionName).listIndexes().toArray();
        
        console.log("FauxDB indexes:", fauxdbIndexes.map(i => i.name));
        console.log("MongoDB indexes:", mongodbIndexes.map(i => i.name));
        
    } catch (error) {
        console.log("⚠️ Index operations error:", error.message);
    }
}

// Test 5: Authentication (if supported)
async function testAuthentication(fauxdbClient, mongodbClient) {
    console.log("\n🔐 Testing Authentication...");
    
    try {
        // Test basic authentication
        const fauxdbAdmin = fauxdbClient.admin();
        const mongodbAdmin = mongodbClient.admin();
        
        // List users (if authentication is enabled)
        try {
            const fauxdbUsers = await fauxdbAdmin.listUsers();
            console.log("FauxDB users found:", fauxdbUsers.users.length);
        } catch (error) {
            console.log("FauxDB auth test:", error.message);
        }
        
        try {
            const mongodbUsers = await mongodbAdmin.listUsers();
            console.log("MongoDB users found:", mongodbUsers.users.length);
        } catch (error) {
            console.log("MongoDB auth test:", error.message);
        }
        
    } catch (error) {
        console.log("⚠️ Authentication test error:", error.message);
    }
}

// Main test execution
async function runAllTests() {
    console.log("🚀 Starting FauxDB vs MongoDB Compatibility Tests");
    console.log("=" * 60);
    
    await runComparisonTest("Basic CRUD Operations", testBasicCRUD);
    await runComparisonTest("Aggregation Pipeline", testAggregationPipeline);
    await runComparisonTest("Geospatial Operations", testGeospatialOperations);
    await runComparisonTest("Index Operations", testIndexOperations);
    await runComparisonTest("Authentication", testAuthentication);
    
    console.log("\n🏁 All tests completed!");
    console.log("=" * 60);
}

// Run the tests
runAllTests().catch(console.error);
