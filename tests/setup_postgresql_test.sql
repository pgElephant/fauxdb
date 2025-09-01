-- PostgreSQL setup script for FauxDB testing
-- This script prepares the PostgreSQL database for comprehensive testing

-- Connect to the fauxdb database
\c fauxdb;

-- Create test collections (tables) with JSONB document storage
CREATE TABLE IF NOT EXISTS "testcoll" (
    _id VARCHAR(24) PRIMARY KEY,
    document JSONB NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create another test table
CREATE TABLE IF NOT EXISTS "users" (
    _id VARCHAR(24) PRIMARY KEY,
    document JSONB NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create products table for testing
CREATE TABLE IF NOT EXISTS "products" (
    _id VARCHAR(24) PRIMARY KEY,
    document JSONB NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert sample test data
INSERT INTO "testcoll" (_id, document) VALUES
    ('1', '{"_id": "1", "name": "Alice", "age": 30, "city": "New York", "department": "Engineering"}'),
    ('2', '{"_id": "2", "name": "Bob", "age": 25, "city": "London", "department": "Marketing"}'),
    ('3', '{"_id": "3", "name": "Charlie", "age": 35, "city": "Tokyo", "department": "Sales"}'),
    ('4', '{"_id": "4", "name": "Diana", "age": 28, "city": "Paris", "department": "Engineering"}'),
    ('5', '{"_id": "5", "name": "Eve", "age": 32, "city": "Berlin", "department": "Finance"}')
ON CONFLICT (_id) DO UPDATE SET 
    document = EXCLUDED.document,
    updated_at = CURRENT_TIMESTAMP;

-- Insert sample users data
INSERT INTO "users" (_id, document) VALUES
    ('u1', '{"_id": "u1", "username": "alice_doe", "email": "alice@example.com", "active": true}'),
    ('u2', '{"_id": "u2", "username": "bob_smith", "email": "bob@example.com", "active": true}'),
    ('u3', '{"_id": "u3", "username": "charlie_brown", "email": "charlie@example.com", "active": false}')
ON CONFLICT (_id) DO UPDATE SET 
    document = EXCLUDED.document,
    updated_at = CURRENT_TIMESTAMP;

-- Insert sample products data
INSERT INTO "products" (_id, document) VALUES
    ('p1', '{"_id": "p1", "name": "Laptop", "price": 999.99, "category": "Electronics", "in_stock": true}'),
    ('p2', '{"_id": "p2", "name": "Phone", "price": 699.99, "category": "Electronics", "in_stock": true}'),
    ('p3', '{"_id": "p3", "name": "Desk", "price": 299.99, "category": "Furniture", "in_stock": false}')
ON CONFLICT (_id) DO UPDATE SET 
    document = EXCLUDED.document,
    updated_at = CURRENT_TIMESTAMP;

-- Create some indexes for testing
CREATE INDEX IF NOT EXISTS idx_testcoll_name ON "testcoll" USING GIN ((document->>'name'));
CREATE INDEX IF NOT EXISTS idx_testcoll_age ON "testcoll" USING BTREE (CAST(document->>'age' AS INTEGER));
CREATE INDEX IF NOT EXISTS idx_testcoll_city ON "testcoll" USING BTREE ((document->>'city'));

CREATE INDEX IF NOT EXISTS idx_users_username ON "users" USING BTREE ((document->>'username'));
CREATE INDEX IF NOT EXISTS idx_users_email ON "users" USING BTREE ((document->>'email'));

CREATE INDEX IF NOT EXISTS idx_products_category ON "products" USING BTREE ((document->>'category'));
CREATE INDEX IF NOT EXISTS idx_products_price ON "products" USING BTREE (CAST(document->>'price' AS NUMERIC));

-- Display table information
\echo '=== PostgreSQL Test Database Setup Complete ==='
\echo 'Tables created with sample data:'

SELECT 'testcoll' as table_name, COUNT(*) as row_count FROM "testcoll"
UNION ALL
SELECT 'users' as table_name, COUNT(*) as row_count FROM "users"
UNION ALL
SELECT 'products' as table_name, COUNT(*) as row_count FROM "products";

\echo ''
\echo 'Indexes created:'
SELECT 
    schemaname,
    tablename, 
    indexname,
    indexdef
FROM pg_indexes 
WHERE tablename IN ('testcoll', 'users', 'products')
AND schemaname = 'public'
ORDER BY tablename, indexname;

\echo ''
\echo '=== Ready for FauxDB testing ==='
