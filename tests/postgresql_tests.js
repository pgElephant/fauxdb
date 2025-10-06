/*!
 * PostgreSQL Backend Tests for FauxDB
 * Tests FauxDB's PostgreSQL backend operations using mongosh
 */

// Test configuration
const FAUXDB_CONNECTION = "mongodb://localhost:27018/test";

// Helper function to test FauxDB connection
async function testFauxDBConnection() {
    try {
        console.log("\n🔌 Testing FauxDB PostgreSQL Backend Connection...");
        const client = new Mongo(FAUXDB_CONNECTION);
        const admin = client.admin();
        const result = await admin.ping();
        console.log("✅ FauxDB connection successful:", result);
        return client;
    } catch (error) {
        console.error("❌ FauxDB connection failed:", error.message);
        return null;
    }
}

// Test 1: PostgreSQL JSONB Operations
async function testPostgreSQLJSONBOperations(client) {
    console.log("\n📄 Testing PostgreSQL JSONB Operations...");
    
    const db = client.db("test");
    const collectionName = "jsonb_test";
    
    // Clean up existing data
    await db.collection(collectionName).drop().catch(() => {});
    
    const complexDoc = {
        _id: ObjectId(),
        user: {
            name: "John Doe",
            profile: {
                age: 30,
                preferences: {
                    theme: "dark",
                    notifications: true,
                    languages: ["en", "es"]
                }
            },
            metadata: {
                created_at: new Date(),
                last_login: new Date(),
                login_count: 42,
                tags: ["premium", "verified"]
            }
        },
        orders: [
            {
                order_id: "ORD-001",
                items: [
                    {product: "laptop", price: 999.99, quantity: 1},
                    {product: "mouse", price: 29.99, quantity: 2}
                ],
                total: 1059.97,
                status: "completed"
            },
            {
                order_id: "ORD-002", 
                items: [
                    {product: "keyboard", price: 79.99, quantity: 1}
                ],
                total: 79.99,
                status: "pending"
            }
        ],
        location: {
            type: "Point",
            coordinates: [-122.4194, 37.7749],
            address: {
                street: "123 Main St",
                city: "San Francisco",
                state: "CA",
                zip: "94102"
            }
        }
    };
    
    // Insert complex document
    console.log("📝 Inserting complex JSONB document...");
    const insertResult = await db.collection(collectionName).insertOne(complexDoc);
    console.log("✅ Document inserted with ID:", insertResult.insertedId);
    
    // Test nested field queries
    console.log("\n🔍 Testing nested field queries...");
    
    // Query by nested field
    const nestedQuery = await db.collection(collectionName).findOne({
        "user.profile.age": {$gte: 25}
    });
    console.log("✅ Nested field query result:", nestedQuery ? "Found" : "Not found");
    
    // Query by array element
    const arrayQuery = await db.collection(collectionName).findOne({
        "user.metadata.tags": "premium"
    });
    console.log("✅ Array element query result:", arrayQuery ? "Found" : "Not found");
    
    // Test aggregation with nested fields
    console.log("\n📊 Testing aggregation with nested fields...");
    const aggregationResult = await db.collection(collectionName).aggregate([
        {$match: {"user.profile.preferences.theme": "dark"}},
        {$project: {
            user_name: "$user.name",
            total_orders: {$size: "$orders"},
            total_amount: {$sum: "$orders.total"}
        }}
    ]).toArray();
    
    console.log("✅ Aggregation result:", JSON.stringify(aggregationResult, null, 2));
    
    // Test update operations on nested fields
    console.log("\n✏️ Testing nested field updates...");
    const updateResult = await db.collection(collectionName).updateOne(
        {_id: insertResult.insertedId},
        {
            $set: {
                "user.profile.preferences.theme": "light",
                "user.metadata.last_login": new Date(),
                "user.metadata.login_count": {$inc: 1}
            },
            $push: {
                "user.metadata.tags": "updated"
            }
        }
    );
    console.log("✅ Nested field update result:", updateResult.modifiedCount, "documents modified");
    
    // Test array operations
    console.log("\n📋 Testing array operations...");
    const arrayUpdateResult = await db.collection(collectionName).updateOne(
        {_id: insertResult.insertedId},
        {
            $push: {
                "orders": {
                    order_id: "ORD-003",
                    items: [{product: "monitor", price: 299.99, quantity: 1}],
                    total: 299.99,
                    status: "processing"
                }
            }
        }
    );
    console.log("✅ Array push result:", arrayUpdateResult.modifiedCount, "documents modified");
    
    // Test geospatial operations
    console.log("\n🗺️ Testing geospatial operations...");
    const geoQuery = await db.collection(collectionName).findOne({
        location: {
            $near: {
                $geometry: {type: "Point", coordinates: [-122.4194, 37.7749]},
                $maxDistance: 1000
            }
        }
    });
    console.log("✅ Geospatial query result:", geoQuery ? "Found nearby location" : "No nearby location");
}

// Test 2: PostgreSQL Index Operations
async function testPostgreSQLIndexOperations(client) {
    console.log("\n📇 Testing PostgreSQL Index Operations...");
    
    const db = client.db("test");
    const collectionName = "index_test";
    
    // Clean up existing data
    await db.collection(collectionName).drop().catch(() => {});
    
    // Insert test data
    const testData = [];
    for (let i = 0; i < 1000; i++) {
        testData.push({
            name: `User${i}`,
            email: `user${i}@example.com`,
            age: Math.floor(Math.random() * 50) + 18,
            score: Math.floor(Math.random() * 1000),
            category: ["A", "B", "C"][Math.floor(Math.random() * 3)],
            created_at: new Date(Date.now() - Math.random() * 365 * 24 * 60 * 60 * 1000),
            metadata: {
                tags: [`tag${i % 10}`, `category${i % 5}`],
                settings: {
                    theme: ["light", "dark"][i % 2],
                    notifications: i % 2 === 0
                }
            }
        });
    }
    
    await db.collection(collectionName).insertMany(testData);
    console.log("✅ Inserted 1000 test documents");
    
    // Create various types of indexes
    console.log("\n🔨 Creating indexes...");
    
    try {
        // Single field index
        await db.collection(collectionName).createIndex({name: 1});
        console.log("✅ Created single field index on 'name'");
        
        // Compound index
        await db.collection(collectionName).createIndex({category: 1, score: -1});
        console.log("✅ Created compound index on 'category' and 'score'");
        
        // Partial index
        await db.collection(collectionName).createIndex({email: 1}, {partialFilterExpression: {age: {$gte: 25}}});
        console.log("✅ Created partial index on 'email' for age >= 25");
        
        // Text index (if supported)
        try {
            await db.collection(collectionName).createIndex({name: "text", email: "text"});
            console.log("✅ Created text index on 'name' and 'email'");
        } catch (error) {
            console.log("⚠️ Text index not supported:", error.message);
        }
        
        // Geospatial index (if location data exists)
        try {
            await db.collection(collectionName).createIndex({location: "2dsphere"});
            console.log("✅ Created 2dsphere index");
        } catch (error) {
            console.log("⚠️ Geospatial index not applicable:", error.message);
        }
        
    } catch (error) {
        console.log("⚠️ Index creation error:", error.message);
    }
    
    // Test query performance with indexes
    console.log("\n⚡ Testing query performance...");
    
    const startTime = Date.now();
    const queryResult = await db.collection(collectionName).find({
        category: "A",
        score: {$gte: 500}
    }).sort({score: -1}).limit(10).toArray();
    const queryTime = Date.now() - startTime;
    
    console.log(`✅ Query completed in ${queryTime}ms, found ${queryResult.length} documents`);
    
    // List all indexes
    const indexes = await db.collection(collectionName).listIndexes().toArray();
    console.log("\n📋 Available indexes:");
    indexes.forEach(index => {
        console.log(`  - ${index.name}: ${JSON.stringify(index.key)}`);
    });
}

// Test 3: PostgreSQL Transaction Operations
async function testPostgreSQLTransactionOperations(client) {
    console.log("\n💳 Testing PostgreSQL Transaction Operations...");
    
    const db = client.db("test");
    const accountsCollection = "accounts";
    const transactionsCollection = "transactions";
    
    // Clean up existing data
    await db.collection(accountsCollection).drop().catch(() => {});
    await db.collection(transactionsCollection).drop().catch(() => {});
    
    // Insert initial account data
    await db.collection(accountsCollection).insertMany([
        {account_id: "ACC001", balance: 1000.00, owner: "Alice"},
        {account_id: "ACC002", balance: 500.00, owner: "Bob"}
    ]);
    
    console.log("✅ Initial accounts created");
    
    // Test transaction simulation
    try {
        const session = client.startSession();
        await session.withTransaction(async () => {
            // Transfer $200 from Alice to Bob
            console.log("🔄 Starting transfer transaction...");
            
            // Debit from Alice's account
            await db.collection(accountsCollection).updateOne(
                {account_id: "ACC001"},
                {$inc: {balance: -200.00}},
                {session}
            );
            
            // Credit to Bob's account
            await db.collection(accountsCollection).updateOne(
                {account_id: "ACC002"},
                {$inc: {balance: 200.00}},
                {session}
            );
            
            // Record the transaction
            await db.collection(transactionsCollection).insertOne({
                transaction_id: ObjectId(),
                from_account: "ACC001",
                to_account: "ACC002",
                amount: 200.00,
                timestamp: new Date(),
                status: "completed"
            }, {session});
            
            console.log("✅ Transaction completed successfully");
        });
        
        // Verify balances after transaction
        const aliceAccount = await db.collection(accountsCollection).findOne({account_id: "ACC001"});
        const bobAccount = await db.collection(accountsCollection).findOne({account_id: "ACC002"});
        
        console.log(`✅ Alice's balance: $${aliceAccount.balance}`);
        console.log(`✅ Bob's balance: $${bobAccount.balance}`);
        
        // List transactions
        const transactions = await db.collection(transactionsCollection).find({}).toArray();
        console.log(`✅ Recorded ${transactions.length} transactions`);
        
    } catch (error) {
        console.log("⚠️ Transaction error:", error.message);
    }
}

// Test 4: PostgreSQL Advanced Queries
async function testPostgreSQLAdvancedQueries(client) {
    console.log("\n🔬 Testing PostgreSQL Advanced Queries...");
    
    const db = client.db("test");
    const collectionName = "advanced_queries_test";
    
    // Clean up existing data
    await db.collection(collectionName).drop().catch(() => {});
    
    // Insert complex test data
    const testData = [
        {
            product: "laptop",
            price: 999.99,
            category: "electronics",
            tags: ["portable", "computing"],
            specifications: {
                cpu: "Intel i7",
                ram: "16GB",
                storage: "512GB SSD"
            },
            reviews: [
                {rating: 5, comment: "Excellent laptop", user: "user1"},
                {rating: 4, comment: "Good performance", user: "user2"}
            ],
            inventory: {stock: 50, warehouse: "A"}
        },
        {
            product: "mouse",
            price: 29.99,
            category: "electronics",
            tags: ["peripheral", "wireless"],
            specifications: {
                type: "wireless",
                dpi: 1600,
                battery_life: "6 months"
            },
            reviews: [
                {rating: 5, comment: "Great mouse", user: "user3"}
            ],
            inventory: {stock: 200, warehouse: "B"}
        },
        {
            product: "keyboard",
            price: 79.99,
            category: "electronics",
            tags: ["peripheral", "mechanical"],
            specifications: {
                type: "mechanical",
                switches: "blue",
                backlight: true
            },
            reviews: [
                {rating: 4, comment: "Nice keyboard", user: "user4"},
                {rating: 5, comment: "Love the feel", user: "user5"}
            ],
            inventory: {stock: 75, warehouse: "A"}
        }
    ];
    
    await db.collection(collectionName).insertMany(testData);
    console.log("✅ Inserted complex test data");
    
    // Test complex aggregation
    console.log("\n📊 Testing complex aggregation...");
    const complexAggregation = await db.collection(collectionName).aggregate([
        // Match electronics with stock > 0
        {$match: {
            category: "electronics",
            "inventory.stock": {$gt: 0}
        }},
        
        // Add computed fields
        {$addFields: {
            total_reviews: {$size: "$reviews"},
            average_rating: {$avg: "$reviews.rating"},
            in_stock: {$gt: ["$inventory.stock", 0]}
        }},
        
        // Group by warehouse
        {$group: {
            _id: "$inventory.warehouse",
            products: {$push: {
                name: "$product",
                price: "$price",
                stock: "$inventory.stock",
                avg_rating: "$average_rating"
            }},
            total_products: {$sum: 1},
            total_stock: {$sum: "$inventory.stock"},
            avg_price: {$avg: "$price"}
        }},
        
        // Sort by total stock
        {$sort: {total_stock: -1}},
        
        // Project final result
        {$project: {
            warehouse: "$_id",
            products: 1,
            total_products: 1,
            total_stock: 1,
            avg_price: {$round: ["$avg_price", 2]}
        }}
    ]).toArray();
    
    console.log("✅ Complex aggregation result:", JSON.stringify(complexAggregation, null, 2));
    
    // Test text search (if supported)
    console.log("\n🔍 Testing text search...");
    try {
        const textSearchResult = await db.collection(collectionName).find({
            $text: {$search: "laptop mouse"}
        }).toArray();
        console.log("✅ Text search result:", textSearchResult.map(d => d.product));
    } catch (error) {
        console.log("⚠️ Text search not supported:", error.message);
    }
    
    // Test regex queries
    console.log("\n🔤 Testing regex queries...");
    const regexResult = await db.collection(collectionName).find({
        product: {$regex: /^l/, $options: "i"}
    }).toArray();
    console.log("✅ Regex query result:", regexResult.map(d => d.product));
}

// Main test execution
async function runPostgreSQLTests() {
    console.log("🚀 Starting FauxDB PostgreSQL Backend Tests");
    console.log("=" * 60);
    
    const client = await testFauxDBConnection();
    if (!client) {
        console.log("❌ Cannot proceed without FauxDB connection");
        return;
    }
    
    try {
        await testPostgreSQLJSONBOperations(client);
        await testPostgreSQLIndexOperations(client);
        await testPostgreSQLTransactionOperations(client);
        await testPostgreSQLAdvancedQueries(client);
        
        console.log("\n🏁 All PostgreSQL backend tests completed!");
        
    } catch (error) {
        console.error("❌ Test execution error:", error);
    }
}

// Run the tests
runPostgreSQLTests().catch(console.error);
