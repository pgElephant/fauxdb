---
layout: default
title: CRUD Operations
parent: API Reference
nav_order: 1
description: Complete reference for MongoDB CRUD operations in FauxDB
---

# CRUD Operations

FauxDB provides full MongoDB-compatible CRUD (Create, Read, Update, Delete) operations. This section covers all supported operations with examples.

## Collection Operations

### Create Collection

```javascript
// Create a collection (implicit)
db.users.insertOne({name: "John"});

// Explicit collection creation
db.createCollection("products");

// Create with options
db.createCollection("logs", {
    capped: true,
    size: 100000,
    max: 1000
});
```

### Drop Collection

```javascript
// Drop a collection
db.users.drop();

// Check if collection exists before dropping
if (db.users.exists()) {
    db.users.drop();
}
```

### List Collections

```javascript
// List all collections
db.getCollectionNames();

// List collections with details
db.runCommand("listCollections");

// List collections matching pattern
db.getCollectionNames().filter(name => name.startsWith("user"));
```

## Insert Operations

### insertOne()

Insert a single document into a collection.

```javascript
// Basic insert
const result = db.users.insertOne({
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
});

console.log(result.insertedId);
```

**Response:**
```javascript
{
    acknowledged: true,
    insertedId: ObjectId("507f1f77bcf86cd799439011")
}
```

### insertMany()

Insert multiple documents into a collection.

```javascript
// Insert multiple documents
const result = db.users.insertMany([
    {
        name: "Alice",
        email: "alice@example.com",
        age: 25
    },
    {
        name: "Bob",
        email: "bob@example.com",
        age: 35
    },
    {
        name: "Charlie",
        email: "charlie@example.com",
        age: 28
    }
]);

console.log(result.insertedIds);
```

**Response:**
```javascript
{
    acknowledged: true,
    insertedIds: [
        ObjectId("507f1f77bcf86cd799439011"),
        ObjectId("507f1f77bcf86cd799439012"),
        ObjectId("507f1f77bcf86cd799439013")
    ]
}
```

### Insert Options

```javascript
// Insert with options
db.users.insertOne(
    {name: "John"},
    {
        writeConcern: {w: 1, j: true},
        ordered: true
    }
);
```

## Query Operations

### find()

Query documents from a collection.

```javascript
// Find all documents
db.users.find();

// Find with filter
db.users.find({age: {$gte: 18}});

// Find with projection
db.users.find(
    {age: {$gte: 18}},
    {name: 1, email: 1, _id: 0}
);

// Find with sort and limit
db.users.find({})
    .sort({age: -1})
    .limit(10)
    .skip(5);
```

### findOne()

Find a single document.

```javascript
// Find first document
db.users.findOne();

// Find with filter
db.users.findOne({email: "john@example.com"});

// Find with projection
db.users.findOne(
    {age: {$gte: 30}},
    {name: 1, email: 1}
);
```

### Query Operators

#### Comparison Operators

```javascript
// Equal
db.users.find({age: 30});

// Not equal
db.users.find({age: {$ne: 30}});

// Greater than
db.users.find({age: {$gt: 25}});

// Greater than or equal
db.users.find({age: {$gte: 25}});

// Less than
db.users.find({age: {$lt: 40}});

// Less than or equal
db.users.find({age: {$lte: 40}});

// In array
db.users.find({age: {$in: [25, 30, 35]}});

// Not in array
db.users.find({age: {$nin: [25, 30, 35]}});
```

#### Logical Operators

```javascript
// AND (implicit)
db.users.find({age: {$gte: 18}, status: "active"});

// AND (explicit)
db.users.find({
    $and: [
        {age: {$gte: 18}},
        {status: "active"}
    ]
});

// OR
db.users.find({
    $or: [
        {age: {$lt: 18}},
        {status: "inactive"}
    ]
});

// NOR
db.users.find({
    $nor: [
        {age: {$lt: 18}},
        {status: "inactive"}
    ]
});

// NOT
db.users.find({
    age: {$not: {$gte: 18}}
});
```

#### Element Operators

```javascript
// Field exists
db.users.find({email: {$exists: true}});

// Field type
db.users.find({age: {$type: "number"}});
db.users.find({name: {$type: "string"}});
```

#### Array Operators

```javascript
// Array contains all elements
db.users.find({tags: {$all: ["developer", "javascript"]}});

// Array element match
db.users.find({
    tags: {
        $elemMatch: {
            $regex: /^dev/
        }
    }
});

// Array size
db.users.find({tags: {$size: 3}});
```

#### Evaluation Operators

```javascript
// Regular expression
db.users.find({email: {$regex: /@example\.com$/}});

// Text search (requires text index)
db.users.find({$text: {$search: "developer javascript"}});

// JavaScript expression
db.users.find({
    $where: "this.age > 18 && this.status === 'active'"
});
```

## Update Operations

### updateOne()

Update a single document.

```javascript
// Update single document
const result = db.users.updateOne(
    {email: "john@example.com"},
    {$set: {age: 31}}
);

console.log(result.matchedCount, result.modifiedCount);
```

**Response:**
```javascript
{
    acknowledged: true,
    matchedCount: 1,
    modifiedCount: 1,
    upsertedId: null
}
```

### updateMany()

Update multiple documents.

```javascript
// Update multiple documents
const result = db.users.updateMany(
    {status: "inactive"},
    {$set: {lastLogin: new Date()}}
);

console.log(result.matchedCount, result.modifiedCount);
```

### replaceOne()

Replace a single document.

```javascript
// Replace document
const result = db.users.replaceOne(
    {email: "john@example.com"},
    {
        name: "John Smith",
        email: "john.smith@example.com",
        age: 32,
        status: "active"
    }
);
```

### Update Operators

#### Field Update Operators

```javascript
// Set field value
db.users.updateOne(
    {_id: ObjectId("...")},
    {$set: {age: 30}}
);

// Unset field
db.users.updateOne(
    {_id: ObjectId("...")},
    {$unset: {tempField: ""}}
);

// Increment field
db.users.updateOne(
    {_id: ObjectId("...")},
    {$inc: {age: 1}}
);

// Multiply field
db.users.updateOne(
    {_id: ObjectId("...")},
    {$mul: {score: 1.1}}
);

// Rename field
db.users.updateOne(
    {_id: ObjectId("...")},
    {$rename: {oldName: "newName"}}
);

// Set on insert
db.users.updateOne(
    {email: "john@example.com"},
    {$setOnInsert: {createdAt: new Date()}},
    {upsert: true}
);
```

#### Array Update Operators

```javascript
// Add to array
db.users.updateOne(
    {_id: ObjectId("...")},
    {$push: {tags: "senior"}}
);

// Add multiple to array
db.users.updateOne(
    {_id: ObjectId("...")},
    {$push: {tags: {$each: ["senior", "lead"]}}}
);

// Remove from array
db.users.updateOne(
    {_id: ObjectId("...")},
    {$pull: {tags: "junior"}}
);

// Remove multiple from array
db.users.updateOne(
    {_id: ObjectId("...")},
    {$pull: {tags: {$in: ["junior", "intern"]}}}
);

// Pop from array
db.users.updateOne(
    {_id: ObjectId("...")},
    {$pop: {tags: 1}}  // 1 = last, -1 = first
);

// Add to set (unique values)
db.users.updateOne(
    {_id: ObjectId("...")},
    {$addToSet: {tags: "developer"}}
);
```

#### Update Options

```javascript
// Upsert (insert if not found)
db.users.updateOne(
    {email: "newuser@example.com"},
    {$set: {name: "New User", age: 25}},
    {upsert: true}
);

// Array filters
db.users.updateOne(
    {_id: ObjectId("...")},
    {$set: {"comments.$[elem].hidden": true}},
    {arrayFilters: [{"elem.author": "spam"}]}
);
```

## Delete Operations

### deleteOne()

Delete a single document.

```javascript
// Delete single document
const result = db.users.deleteOne({email: "john@example.com"});

console.log(result.deletedCount);
```

**Response:**
```javascript
{
    acknowledged: true,
    deletedCount: 1
}
```

### deleteMany()

Delete multiple documents.

```javascript
// Delete multiple documents
const result = db.users.deleteMany({status: "inactive"});

console.log(result.deletedCount);
```

### Delete Options

```javascript
// Delete with options
db.users.deleteOne(
    {email: "john@example.com"},
    {
        writeConcern: {w: 1, j: true}
    }
);
```

## Bulk Operations

### bulkWrite()

Perform multiple write operations.

```javascript
// Bulk write operations
const result = db.users.bulkWrite([
    {
        insertOne: {
            document: {
                name: "Alice",
                email: "alice@example.com",
                age: 25
            }
        }
    },
    {
        updateOne: {
            filter: {email: "bob@example.com"},
            update: {$set: {age: 36}}
        }
    },
    {
        deleteOne: {
            filter: {email: "charlie@example.com"}
        }
    }
]);

console.log(result.insertedCount, result.modifiedCount, result.deletedCount);
```

## Count Operations

### countDocuments()

Count documents matching a filter.

```javascript
// Count all documents
const total = db.users.countDocuments();

// Count with filter
const active = db.users.countDocuments({status: "active"});

// Count with options
const result = db.users.countDocuments(
    {age: {$gte: 18}},
    {limit: 100}
);
```

### estimatedDocumentCount()

Get estimated document count (faster but less accurate).

```javascript
// Estimated count
const estimate = db.users.estimatedDocumentCount();
```

## Distinct Values

### distinct()

Get distinct values for a field.

```javascript
// Get distinct values
const ages = db.users.distinct("age");

// Get distinct with filter
const activeAges = db.users.distinct("age", {status: "active"});
```

## Error Handling

### Write Errors

```javascript
try {
    const result = db.users.insertOne({_id: "duplicate"});
} catch (error) {
    if (error.code === 11000) {
        console.log("Duplicate key error");
    } else {
        console.log("Other error:", error.message);
    }
}
```

### Read Errors

```javascript
try {
    const result = db.users.find({invalidQuery: {$invalid: "operator"}});
} catch (error) {
    console.log("Query error:", error.message);
}
```

## Performance Tips

### Query Optimization

```javascript
// Use indexes for frequently queried fields
db.users.createIndex({email: 1});
db.users.createIndex({age: 1, status: 1});

// Use projection to limit returned fields
db.users.find({}, {name: 1, email: 1});

// Use limit() to restrict results
db.users.find({}).limit(100);

// Use sort() with indexed fields
db.users.find({}).sort({email: 1});
```

### Write Optimization

```javascript
// Use bulk operations for multiple writes
db.users.bulkWrite([
    {insertOne: {document: {...}}},
    {updateOne: {filter: {...}, update: {...}}}
]);

// Use ordered: false for parallel execution
db.users.insertMany(documents, {ordered: false});
```

## Transaction Support

### Single Document Transactions

```javascript
// Start session
const session = db.getMongo().startSession();

try {
    // Start transaction
    session.startTransaction();
    
    // Perform operations
    db.users.insertOne({name: "John"}, {session});
    db.profiles.insertOne({userId: "123"}, {session});
    
    // Commit transaction
    session.commitTransaction();
} catch (error) {
    // Abort transaction
    session.abortTransaction();
    throw error;
} finally {
    session.endSession();
}
```

## Next Steps

- [Query Operators]({{ site.baseurl }}/api/operations/queries/) - Advanced query operators
- [Aggregation Pipeline]({{ site.baseurl }}/api/operations/aggregation/) - Complex data processing
- [Indexes]({{ site.baseurl }}/api/operations/indexes/) - Performance optimization
