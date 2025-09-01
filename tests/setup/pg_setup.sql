-- FauxDB Test Database Setup
-- Drop and recreate tables for consistent testing

DROP TABLE IF EXISTS test CASCADE;
DROP TABLE IF EXISTS users CASCADE;
DROP TABLE IF EXISTS products CASCADE;
DROP TABLE IF EXISTS orders CASCADE;

-- Create test table with 100 rows (as expected by tests)
CREATE TABLE test (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    value INTEGER,
    active BOOLEAN DEFAULT true,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert 100 test records
INSERT INTO test (name, value, active) 
SELECT 
    'test_record_' || i,
    (i * 10) % 1000,
    (i % 2 = 0)
FROM generate_series(1, 100) i;

-- Create users table
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100),
    age INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO users (username, email, age) VALUES
    ('alice', 'alice@example.com', 25),
    ('bob', 'bob@example.com', 30),
    ('charlie', 'charlie@example.com', 35);

-- Create products table
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10,2),
    category VARCHAR(50),
    in_stock BOOLEAN DEFAULT true
);

INSERT INTO products (name, price, category, in_stock) VALUES
    ('Laptop', 999.99, 'electronics', true),
    ('Book', 19.99, 'books', true),
    ('Coffee', 4.99, 'food', false);

-- Verify setup
SELECT 'test' as table_name, COUNT(*) as row_count FROM test
UNION ALL
SELECT 'users', COUNT(*) FROM users  
UNION ALL
SELECT 'products', COUNT(*) FROM products;
