-- FauxDB PostgreSQL Initialization Script
-- Copyright (c) 2025 pgElephant. All rights reserved.

-- Enable required extensions
CREATE EXTENSION IF NOT EXISTS btree_gin;
CREATE EXTENSION IF NOT EXISTS btree_gist;
CREATE EXTENSION IF NOT EXISTS pg_trgm;
CREATE EXTENSION IF NOT EXISTS unaccent;

-- Create FauxDB user (if not exists)
DO $$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_catalog.pg_roles WHERE rolname = 'fauxdb') THEN
        CREATE ROLE fauxdb WITH LOGIN PASSWORD 'fauxdb123';
    END IF;
END
$$;

-- Grant necessary permissions
GRANT ALL PRIVILEGES ON DATABASE fauxdb TO fauxdb;
GRANT ALL PRIVILEGES ON DATABASE fauxdb_dev TO fauxdb;

-- Create default schema for FauxDB
CREATE SCHEMA IF NOT EXISTS fauxdb_public;
GRANT ALL ON SCHEMA fauxdb_public TO fauxdb;

-- Create a sample collection for testing
CREATE TABLE IF NOT EXISTS fauxdb_public.test_collections (
    id SERIAL PRIMARY KEY,
    document JSONB,
    bson_document BYTEA,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create indexes for better performance
CREATE INDEX IF NOT EXISTS idx_test_document_gin ON fauxdb_public.test_collections USING GIN (document);
CREATE INDEX IF NOT EXISTS idx_test_created_at ON fauxdb_public.test_collections (created_at);
CREATE INDEX IF NOT EXISTS idx_test_updated_at ON fauxdb_public.test_collections (updated_at);

-- Insert sample data
INSERT INTO fauxdb_public.test_collections (document) VALUES 
    ('{"name": "FauxDB", "version": "1.0.0", "type": "database", "features": ["mongodb-compatible", "postgresql-backend"]}'),
    ('{"name": "PostgreSQL", "version": "17.0", "type": "backend", "features": ["jsonb", "reliability"]}'),
    ('{"name": "Rust", "version": "1.70", "type": "language", "features": ["performance", "safety"]}'),
    ('{"name": "MongoDB", "version": "5.0+", "type": "compatibility", "features": ["wire-protocol", "bson"]}')
ON CONFLICT DO NOTHING;

-- Grant permissions on the table
GRANT ALL PRIVILEGES ON TABLE fauxdb_public.test_collections TO fauxdb;
GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA fauxdb_public TO fauxdb;

-- Create a function to get FauxDB status
CREATE OR REPLACE FUNCTION fauxdb_public.get_status()
RETURNS JSONB AS $$
BEGIN
    RETURN jsonb_build_object(
        'status', 'running',
        'version', '1.0.0',
        'backend', 'PostgreSQL',
        'timestamp', NOW(),
        'collections', (
            SELECT COUNT(*) 
            FROM information_schema.tables 
            WHERE table_schema LIKE 'fauxdb_%'
        )
    );
END;
$$ LANGUAGE plpgsql;

-- Grant execute permission on the function
GRANT EXECUTE ON FUNCTION fauxdb_public.get_status() TO fauxdb;

-- Log successful initialization
DO $$
BEGIN
    RAISE NOTICE 'FauxDB PostgreSQL initialization completed successfully!';
    RAISE NOTICE 'Sample data inserted into test_collections';
    RAISE NOTICE 'FauxDB is ready to accept connections';
END
$$;
