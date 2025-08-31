# Usage Guide

## Quick Start

### Connecting to FauxDB

FauxDB presents a MongoDB-compatible interface, so you can use any MongoDB client or driver:

```bash
# Using mongosh (MongoDB shell)
mongosh --host localhost --port 27018

# Using MongoDB URI
mongosh "mongodb://localhost:27018/myapp"
```

### Basic Operations

#### Database and Collection Operations

```javascript
// Switch to a database
use myapp

// Show current database
db

// List databases
show dbs

// List collections
show collections

// Get collection
db.users

// Collection stats
db.users.stats()
```

## CRUD Operations

### Create (Insert)

#### Insert Single Document
```javascript
// Insert one document
db.users.insertOne({
    name: "John Doe",
    email: "john@example.com",
    age: 30,
    status: "active",
    created: new Date()
})

// Result:
// {
//   "acknowledged": true,
//   "insertedId": ObjectId("...")
// }
```

#### Insert Multiple Documents
```javascript
// Insert multiple documents
db.users.insertMany([
    {
        name: "Jane Smith",
        email: "jane@example.com",
        age: 28,
        department: "Engineering"
    },
    {
        name: "Bob Johnson",
        email: "bob@example.com",
        age: 35,
        department: "Sales"
    }
])
```

### Read (Query)

#### Basic Queries
```javascript
// Find all documents
db.users.find()

// Find with filter
db.users.find({ age: 30 })

// Find one document
db.users.findOne({ email: "john@example.com" })

// Find with multiple conditions
db.users.find({
    age: { $gte: 25 },
    status: "active"
})
```

#### Query Operators
```javascript
// Comparison operators
db.users.find({ age: { $gt: 25 } })          // Greater than
db.users.find({ age: { $gte: 25 } })         // Greater than or equal
db.users.find({ age: { $lt: 40 } })          // Less than
db.users.find({ age: { $lte: 40 } })         // Less than or equal
db.users.find({ age: { $ne: 30 } })          // Not equal
db.users.find({ age: { $in: [25, 30, 35] } }) // In array
db.users.find({ age: { $nin: [25, 30] } })   // Not in array

// Logical operators
db.users.find({
    $and: [
        { age: { $gte: 25 } },
        { status: "active" }
    ]
})

db.users.find({
    $or: [
        { department: "Engineering" },
        { department: "Sales" }
    ]
})

// Element operators
db.users.find({ email: { $exists: true } })   // Field exists
db.users.find({ age: { $type: "number" } })   // Field type
```

#### Projection
```javascript
// Select specific fields
db.users.find(
    { age: { $gte: 25 } },
    { name: 1, email: 1 }  // Include name and email
)

// Exclude fields
db.users.find(
    { age: { $gte: 25 } },
    { password: 0 }  // Exclude password field
)
```

#### Sorting and Limiting
```javascript
// Sort by age (ascending)
db.users.find().sort({ age: 1 })

// Sort by age (descending)
db.users.find().sort({ age: -1 })

// Multiple sort fields
db.users.find().sort({ department: 1, age: -1 })

// Limit results
db.users.find().limit(10)

// Skip and limit (pagination)
db.users.find().skip(20).limit(10)

// Count documents
db.users.find({ age: { $gte: 25 } }).count()
```

### Update

#### Update Single Document
```javascript
// Update one document
db.users.updateOne(
    { email: "john@example.com" },
    { $set: { age: 31, lastLogin: new Date() } }
)

// Update operators
db.users.updateOne(
    { email: "john@example.com" },
    {
        $set: { status: "premium" },      // Set field value
        $inc: { loginCount: 1 },          // Increment number
        $unset: { temporaryField: "" },   // Remove field
        $push: { tags: "vip" },           // Add to array
        $pull: { tags: "basic" }          // Remove from array
    }
)
```

#### Update Multiple Documents
```javascript
// Update multiple documents
db.users.updateMany(
    { department: "Sales" },
    { $set: { bonus: 1000 } }
)

// Replace entire document
db.users.replaceOne(
    { email: "john@example.com" },
    {
        name: "John Doe",
        email: "john@example.com",
        age: 31,
        status: "premium",
        updated: new Date()
    }
)
```

#### Upsert (Update or Insert)
```javascript
// Update if exists, insert if not
db.users.updateOne(
    { email: "new@example.com" },
    {
        $set: {
            name: "New User",
            email: "new@example.com",
            created: new Date()
        }
    },
    { upsert: true }
)
```

### Delete

#### Delete Single Document
```javascript
// Delete one document
db.users.deleteOne({ email: "old@example.com" })
```

#### Delete Multiple Documents
```javascript
// Delete multiple documents
db.users.deleteMany({ status: "inactive" })

// Delete all documents in collection
db.users.deleteMany({})
```

## Advanced Queries

### Aggregation Pipeline

#### Basic Aggregation
```javascript
// Group by department and count
db.users.aggregate([
    {
        $group: {
            _id: "$department",
            count: { $sum: 1 },
            avgAge: { $avg: "$age" }
        }
    }
])

// Match and project
db.users.aggregate([
    { $match: { age: { $gte: 25 } } },
    {
        $project: {
            name: 1,
            email: 1,
            ageGroup: {
                $cond: {
                    if: { $gte: ["$age", 30] },
                    then: "senior",
                    else: "junior"
                }
            }
        }
    }
])
```

#### Complex Aggregation
```javascript
// Multi-stage pipeline
db.orders.aggregate([
    // Match orders from last month
    {
        $match: {
            orderDate: {
                $gte: new Date("2024-01-01"),
                $lt: new Date("2024-02-01")
            }
        }
    },
    // Group by customer
    {
        $group: {
            _id: "$customerId",
            totalAmount: { $sum: "$amount" },
            orderCount: { $sum: 1 },
            avgOrderValue: { $avg: "$amount" }
        }
    },
    // Sort by total amount
    { $sort: { totalAmount: -1 } },
    // Limit to top 10
    { $limit: 10 },
    // Add customer details
    {
        $lookup: {
            from: "users",
            localField: "_id",
            foreignField: "_id",
            as: "customerInfo"
        }
    }
])
```

### Text Search

```javascript
// Create text index (if supported)
db.articles.createIndex({ title: "text", content: "text" })

// Text search
db.articles.find({ $text: { $search: "mongodb database" } })

// Text search with score
db.articles.find(
    { $text: { $search: "mongodb" } },
    { score: { $meta: "textScore" } }
).sort({ score: { $meta: "textScore" } })
```

### Geospatial Queries

```javascript
// Find locations near a point
db.places.find({
    location: {
        $near: {
            $geometry: {
                type: "Point",
                coordinates: [-73.9857, 40.7484]  // NYC coordinates
            },
            $maxDistance: 1000  // meters
        }
    }
})

// Find locations within a polygon
db.places.find({
    location: {
        $geoWithin: {
            $geometry: {
                type: "Polygon",
                coordinates: [[
                    [-74, 40.5],
                    [-74, 41],
                    [-73, 41],
                    [-73, 40.5],
                    [-74, 40.5]
                ]]
            }
        }
    }
})
```

## Index Management

### Creating Indexes

```javascript
// Single field index
db.users.createIndex({ email: 1 })

// Compound index
db.users.createIndex({ department: 1, age: -1 })

// Unique index
db.users.createIndex({ email: 1 }, { unique: true })

// Partial index
db.users.createIndex(
    { email: 1 },
    { partialFilterExpression: { status: "active" } }
)

// TTL index (time-to-live)
db.sessions.createIndex(
    { createdAt: 1 },
    { expireAfterSeconds: 3600 }  // 1 hour
)
```

### Index Information

```javascript
// List all indexes
db.users.getIndexes()

// Explain query execution
db.users.find({ email: "john@example.com" }).explain("executionStats")

// Drop index
db.users.dropIndex({ email: 1 })
db.users.dropIndex("email_1")  // by name
```

## Transactions (if supported)

```javascript
// Start session
const session = db.getMongo().startSession()

try {
    session.startTransaction()
    
    // Perform operations within transaction
    db.accounts.updateOne(
        { _id: "account1" },
        { $inc: { balance: -100 } },
        { session }
    )
    
    db.accounts.updateOne(
        { _id: "account2" },
        { $inc: { balance: 100 } },
        { session }
    )
    
    // Commit transaction
    session.commitTransaction()
} catch (error) {
    // Abort transaction on error
    session.abortTransaction()
    throw error
} finally {
    session.endSession()
}
```

## Error Handling

### Common Error Scenarios

```javascript
// Handle duplicate key errors
try {
    db.users.insertOne({ email: "existing@example.com" })
} catch (error) {
    if (error.code === 11000) {  // Duplicate key error
        print("User with this email already exists")
    }
}

// Handle validation errors
try {
    db.users.insertOne({ name: "" })  // Empty name
} catch (error) {
    print("Validation error: " + error.message)
}
```

## Performance Tips

### Query Optimization

```javascript
// Use indexes for filtering
db.users.find({ email: "john@example.com" })  // Index on email

// Use projection to limit data transfer
db.users.find({}, { name: 1, email: 1 })

// Use limit to avoid large result sets
db.users.find().limit(100)

// Use aggregation for complex operations
db.users.aggregate([
    { $match: { age: { $gte: 25 } } },
    { $group: { _id: "$department", count: { $sum: 1 } } }
])
```

### Bulk Operations

```javascript
// Bulk insert
const bulk = db.users.initializeOrderedBulkOp()
for (let i = 0; i < 1000; i++) {
    bulk.insert({ name: `User ${i}`, number: i })
}
bulk.execute()

// Bulk update
db.users.bulkWrite([
    {
        updateOne: {
            filter: { name: "John" },
            update: { $set: { status: "active" } }
        }
    },
    {
        insertOne: {
            document: { name: "New User", status: "pending" }
        }
    }
])
```

## Monitoring and Debugging

### Database Statistics

```javascript
// Database stats
db.stats()

// Collection stats
db.users.stats()

// Server status
db.serverStatus()

// Current operations
db.currentOp()
```

### Query Profiling

```javascript
// Enable profiling for slow queries
db.setProfilingLevel(1, { slowms: 100 })

// View profiler collection
db.system.profile.find().sort({ ts: -1 }).limit(5)

// Disable profiling
db.setProfilingLevel(0)
```

## Application Integration

### Node.js Example

```javascript
const { MongoClient } = require('mongodb')

async function main() {
    const client = new MongoClient('mongodb://localhost:27018')
    
    try {
        await client.connect()
        const db = client.db('myapp')
        const users = db.collection('users')
        
        // Insert user
        const result = await users.insertOne({
            name: 'Alice',
            email: 'alice@example.com',
            age: 28
        })
        
        // Find users
        const userList = await users.find({ age: { $gte: 25 } }).toArray()
        console.log(userList)
        
    } finally {
        await client.close()
    }
}

main().catch(console.error)
```

### Python Example

```python
from pymongo import MongoClient
from datetime import datetime

# Connect to FauxDB
client = MongoClient('mongodb://localhost:27018')
db = client.myapp
users = db.users

# Insert document
result = users.insert_one({
    'name': 'Bob',
    'email': 'bob@example.com',
    'age': 32,
    'created': datetime.now()
})

# Query documents
for user in users.find({'age': {'$gte': 25}}):
    print(user)

# Update document
users.update_one(
    {'email': 'bob@example.com'},
    {'$set': {'status': 'premium'}}
)

client.close()
```

## Best Practices

### Schema Design

1. **Embed vs Reference**: Use embedding for data that's always accessed together
2. **Indexing**: Create indexes for frequently queried fields
3. **Field Naming**: Use consistent, descriptive field names
4. **Data Types**: Use appropriate data types for better performance

### Query Performance

1. **Use Indexes**: Ensure queries use appropriate indexes
2. **Limit Results**: Use `limit()` to avoid large result sets
3. **Project Fields**: Only select needed fields with projection
4. **Batch Operations**: Use bulk operations for multiple writes

### Error Handling

1. **Validate Input**: Check data before inserting/updating
2. **Handle Errors**: Implement proper error handling
3. **Use Transactions**: For operations requiring consistency
4. **Monitor Performance**: Track slow queries and optimize

## Troubleshooting

### Connection Issues

```bash
# Test connection
mongosh --host localhost --port 27018 --eval "db.adminCommand('ping')"

# Check if FauxDB is running
ps aux | grep fauxdb
netstat -tlnp | grep 27018
```

### Query Issues

```javascript
// Explain query execution
db.users.find({ email: "test@example.com" }).explain("executionStats")

// Check indexes
db.users.getIndexes()

// Profile slow queries
db.setProfilingLevel(2)  // Profile all operations
```

### Performance Issues

```javascript
// Check database stats
db.stats()

// Monitor current operations
db.currentOp()

// Check index usage
db.users.find().hint({ email: 1 }).explain("executionStats")
```

## Next Steps

- [Architecture Overview](architecture.md) - Understand how FauxDB works internally
- [Performance Tuning](performance.md) - Optimize your FauxDB deployment
- [Monitoring Setup](monitoring.md) - Set up monitoring and alerting
- [Development Guide](development.md) - Contributing to FauxDB
