-- Basic BSON functionality tests
-- Test basic type creation, input/output, and simple operations

-- Create extension if not exists
CREATE EXTENSION IF NOT EXISTS bson;

-- Test basic type creation
SELECT '{}'::bson;
SELECT '[]'::bson;
SELECT '"hello"'::bson;
SELECT '123'::bson;
SELECT 'true'::bson;
SELECT 'null'::bson;

-- Test basic operators
SELECT '{"name": "test"}'::bson -> 'name';
SELECT '{"name": "test"}'::bson ->> 'name';
SELECT '{"name": "test"}'::bson ? 'name';
SELECT '{"name": "test"}'::bson ? 'missing';

-- Test array operations
SELECT '["a", "b", "c"]'::bson -> '0';
SELECT '["a", "b", "c"]'::bson ->> '0';

-- Test nested objects
SELECT '{"user": {"name": "john", "age": 30}}'::bson -> 'user' -> 'name';
SELECT '{"user": {"name": "john", "age": 30}}'::bson -> 'user' ->> 'age';

-- Test comparison operators
SELECT '{"a": 1}'::bson = '{"a": 1}'::bson;
SELECT '{"a": 1}'::bson != '{"a": 2}'::bson;

-- Test containment operators
SELECT '{"a": 1, "b": 2}'::bson @> '{"a": 1}'::bson;
SELECT '{"a": 1}'::bson <@ '{"a": 1, "b": 2}'::bson;

-- Test existence operators
SELECT '{"a": 1, "b": 2}'::bson ?| ARRAY['a', 'c'];
SELECT '{"a": 1, "b": 2}'::bson ?& ARRAY['a', 'b'];
