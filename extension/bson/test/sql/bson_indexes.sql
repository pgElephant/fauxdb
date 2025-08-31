-- BSON indexing tests
-- Test various index types and query performance

-- Create test table
CREATE TABLE bson_test (
    id SERIAL PRIMARY KEY,
    data bson
);

-- Insert test data
INSERT INTO bson_test (data) VALUES
    ('{"name": "john", "age": 30, "city": "new york"}'::bson),
    ('{"name": "jane", "age": 25, "city": "los angeles"}'::bson),
    ('{"name": "bob", "age": 35, "city": "chicago"}'::bson),
    ('{"name": "alice", "age": 28, "city": "boston"}'::bson),
    ('{"name": "charlie", "age": 32, "city": "seattle"}'::bson);

-- Test B-tree index
CREATE INDEX idx_bson_test_btree ON bson_test USING btree (data);

-- Test queries using B-tree index
EXPLAIN (COSTS OFF) SELECT * FROM bson_test WHERE data = '{"name": "john", "age": 30, "city": "new york"}'::bson;
SELECT * FROM bson_test WHERE data = '{"name": "john", "age": 30, "city": "new york"}'::bson;

-- Test hash index
CREATE INDEX idx_bson_test_hash ON bson_test USING hash (data);

-- Test queries using hash index
EXPLAIN (COSTS OFF) SELECT * FROM bson_test WHERE data = '{"name": "jane", "age": 25, "city": "los angeles"}'::bson;
SELECT * FROM bson_test WHERE data = '{"name": "jane", "age": 25, "city": "los angeles"}'::bson;

-- Test GIN index (if supported)
-- CREATE INDEX idx_bson_test_gin ON bson_test USING gin (data);

-- Test containment queries
EXPLAIN (COSTS OFF) SELECT * FROM bson_test WHERE data @> '{"city": "new york"}'::bson;
SELECT * FROM bson_test WHERE data @> '{"city": "new york"}'::bson;

-- Test existence queries
EXPLAIN (COSTS OFF) SELECT * FROM bson_test WHERE data ? 'age';
SELECT * FROM bson_test WHERE data ? 'age';

-- Test comparison queries
EXPLAIN (COSTS OFF) SELECT * FROM bson_test WHERE data > '{"name": "a"}'::bson;
SELECT * FROM bson_test WHERE data > '{"name": "a"}'::bson;

-- Test path queries
EXPLAIN (COSTS OFF) SELECT * FROM bson_test WHERE data->>'age'::int > 30;
SELECT * FROM bson_test WHERE data->>'age'::int > 30;

-- Clean up
DROP TABLE bson_test CASCADE;
