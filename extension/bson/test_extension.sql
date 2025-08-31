-- BSON Extension Test Script
-- Run this after starting PostgreSQL and installing the extension

-- Create the extension
CREATE EXTENSION IF NOT EXISTS bson;

-- Test basic BSON type creation
SELECT 'Basic BSON type test' as test_name;

-- Test BSON input/output functions
SELECT bson_in('{"name": "test", "value": 42}') as bson_value;

-- Test BSON operators
SELECT '{"a": 1, "b": 2}'::bson -> 'a' as get_field;
SELECT '{"a": 1, "b": 2}'::bson ->> 'a' as get_text_field;

-- Test BSON comparison operators
SELECT '{"a": 1}'::bson = '{"a": 1}'::bson as equality_test;
SELECT '{"a": 1}'::bson != '{"a": 2}'::bson as inequality_test;

-- Test BSON containment operators
SELECT '{"a": 1, "b": 2}'::bson @> '{"a": 1}'::bson as contains_test;
SELECT '{"a": 1}'::bson <@ '{"a": 1, "b": 2}'::bson as contained_test;

-- Test BSON existence operators
SELECT '{"a": 1, "b": 2}'::bson ? 'a' as exists_test;
SELECT '{"a": 1, "b": 2}'::bson ? 'c' as not_exists_test;

-- Test BSON query operators
SELECT bson_eq_op('{"a": 1}'::bson, '{"a": 1}'::bson) as eq_op_test;
SELECT bson_ne_op('{"a": 1}'::bson, '{"a": 2}'::bson) as ne_op_test;
SELECT bson_gt_op('{"a": 5}'::bson, '{"a": 3}'::bson) as gt_op_test;
SELECT bson_lt_op('{"a": 2}'::bson, '{"a": 5}'::bson) as lt_op_test;

-- Test BSON array operators
SELECT bson_in_op('{"a": 1}'::bson, '{"values": [1, 2, 3]}'::bson) as in_op_test;
SELECT bson_nin_op('{"a": 4}'::bson, '{"values": [1, 2, 3]}'::bson) as nin_op_test;
SELECT bson_all_op('{"a": [1, 2]}'::bson, '{"values": [1, 2, 3]}'::bson) as all_op_test;

-- Test BSON size operator
SELECT bson_size_op('{"a": [1, 2, 3]}'::bson, '{"size": 3}'::bson) as size_op_test;

-- Test BSON type operator
SELECT bson_type_op('{"a": "string"}'::bson, '{"type": "string"}'::bson) as type_op_test;

-- Test BSON regex operator
SELECT bson_regex_op('{"a": "hello world"}'::bson, '{"pattern": "hello.*"}'::bson) as regex_op_test;

-- Test BSON exists operator
SELECT bson_exists_op('{"a": 1, "b": 2}'::bson, '{"field": "a"}'::bson) as exists_op_test;

-- Test BSON exists_any operator
SELECT bson_exists_any('{"a": 1, "b": 2}'::bson, ARRAY['a', 'c']) as exists_any_test;

-- Test BSON exists_all operator
SELECT bson_exists_all('{"a": 1, "b": 2}'::bson, ARRAY['a', 'b']) as exists_all_test;

-- Test table creation with BSON type
CREATE TABLE test_bson (
    id serial PRIMARY KEY,
    data bson
);

-- Insert BSON data
INSERT INTO test_bson (data) VALUES 
    ('{"name": "John", "age": 30, "city": "New York"}'::bson),
    ('{"name": "Jane", "age": 25, "city": "Los Angeles"}'::bson),
    ('{"name": "Bob", "age": 35, "city": "Chicago"}'::bson);

-- Query BSON data
SELECT id, data->>'name' as name, data->>'age' as age 
FROM test_bson 
WHERE data->>'city' = 'New York';

-- Test BSON indexing
CREATE INDEX idx_test_bson_data ON test_bson USING btree (data);

-- Test BSON hash indexing
CREATE INDEX idx_test_bson_hash ON test_bson USING hash (data);

-- Clean up
DROP TABLE test_bson;
DROP EXTENSION bson;

SELECT 'All tests completed successfully!' as result;
