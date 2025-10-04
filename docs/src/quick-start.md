# Quick Start Guide

Get FauxDB up and running in under 5 minutes.

## Prerequisites

- Rust 1.70+ (for building from source)
- PostgreSQL 13+ (or use Docker)
- MongoDB client (`mongosh`) for testing

## Installation

### Option 1: From Source (Recommended)

```bash
# Clone the repository
git clone https://github.com/pgelephant/fauxdb.git
cd fauxdb

# Build FauxDB
cargo build --release

# Install globally
cargo install --path .
```

### Option 2: Using Cargo

```bash
cargo install fauxdb
```

### Option 3: Docker

```bash
docker pull pgelephant/fauxdb:latest
```

## Database Setup

### Using Docker PostgreSQL

```bash
# Start PostgreSQL
docker run --name postgres-fauxdb \
  -e POSTGRES_DB=fauxdb \
  -e POSTGRES_USER=fauxdb \
  -e POSTGRES_PASSWORD=fauxdb \
  -p 5432:5432 \
  -d postgres:15
```

### Using Local PostgreSQL

```bash
# Create database and user
sudo -u postgres createdb fauxdb
sudo -u postgres createuser -P fauxdb
sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE fauxdb TO fauxdb;"
```

## Configuration

Create a basic configuration file:

```toml
# config/fauxdb.toml
[server]
host = "127.0.0.1"
port = 27017
max_connections = 100

[database]
connection_string = "postgresql://fauxdb:fauxdb@localhost:5432/fauxdb"

[security]
enable_auth = false

[monitoring]
metrics_enabled = true
health_check_interval = 30

[logging]
level = "info"
format = "json"
```

## Start FauxDB

```bash
# Start with configuration file
fauxdb --config config/fauxdb.toml

# Or start with environment variables
FAUXDB_DATABASE_URL="postgresql://fauxdb:fauxdb@localhost:5432/fauxdb" \
FAUXDB_SERVER_PORT=27017 \
fauxdb
```

## Test the Connection

```bash
# Connect with mongosh
mongosh mongodb://localhost:27017

# Test basic operations
> db.test.insertOne({name: "FauxDB", version: "1.0.0"})
> db.test.find()
> db.test.countDocuments()
```

## Example: Basic CRUD Operations

```javascript
// Connect to FauxDB
const client = new MongoClient("mongodb://localhost:27017");
await client.connect();
const db = client.db("test");

// Insert documents
await db.collection("users").insertOne({
  name: "Alice",
  email: "alice@example.com",
  age: 30
});

await db.collection("users").insertMany([
  { name: "Bob", email: "bob@example.com", age: 25 },
  { name: "Charlie", email: "charlie@example.com", age: 35 }
]);

// Query documents
const users = await db.collection("users").find({ age: { $gte: 30 } }).toArray();
console.log("Users 30+:", users);

// Update documents
await db.collection("users").updateOne(
  { name: "Alice" },
  { $set: { age: 31 } }
);

// Delete documents
await db.collection("users").deleteOne({ name: "Charlie" });

// Count documents
const count = await db.collection("users").countDocuments();
console.log("Total users:", count);
```

## Example: Aggregation Pipeline

```javascript
// Aggregation example
const pipeline = [
  { $match: { age: { $gte: 25 } } },
  { $group: { 
      _id: null, 
      averageAge: { $avg: "$age" },
      count: { $sum: 1 }
    }
  }
];

const result = await db.collection("users").aggregate(pipeline).toArray();
console.log("Statistics:", result[0]);
```

## Production Considerations

For production deployments, consider:

1. **Security**: Enable authentication and SSL/TLS
2. **Monitoring**: Set up metrics collection and alerting
3. **Backup**: Configure regular database backups
4. **Scaling**: Use connection pooling and read replicas
5. **Logging**: Configure structured logging

See the [Production Guide](production-guide.md) for detailed instructions.

## Troubleshooting

### Common Issues

**Connection refused:**
```bash
# Check if FauxDB is running
ps aux | grep fauxdb

# Check PostgreSQL connection
psql -h localhost -U fauxdb -d fauxdb
```

**Authentication failed:**
```bash
# Check PostgreSQL user and permissions
sudo -u postgres psql -c "\du"
```

**Port already in use:**
```bash
# Find process using port 27017
sudo lsof -i :27017

# Use different port in config
[server]
port = 27018
```

## Next Steps

- 📖 Read the [Installation Guide](installation.md) for detailed setup
- ⚙️ Explore [Configuration Options](configuration.md)
- 🚀 Check out the [Production Guide](production-guide.md)
- 📚 Browse [API Reference](api/index.md)
- 💡 See more [Examples](examples/index.md)

---

**Need help?** Check out our [Troubleshooting Guide](troubleshooting.md) or open an issue on [GitHub](https://github.com/pgelephant/fauxdb/issues).
