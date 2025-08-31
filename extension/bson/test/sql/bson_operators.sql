-- BSON operators functionality tests

-- Test BSON query operators
SELECT bson_eq_op('{"a": 1}'::bson, '{"a": 1}'::bson);
SELECT bson_ne_op('{"a": 1}'::bson, '{"a": 2}'::bson);
SELECT bson_gt_op('{"a": 2}'::bson, '{"a": 1}'::bson);
SELECT bson_lt_op('{"a": 1}'::bson, '{"a": 2}'::bson);
SELECT bson_gte_op('{"a": 2}'::bson, '{"a": 2}'::bson);
SELECT bson_lte_op('{"a": 1}'::bson, '{"a": 2}'::bson);

-- Test array operators
SELECT bson_in_op('{"a": 1}'::bson, ARRAY['{"a": 1}'::bson, '{"a": 2}'::bson]);
SELECT bson_nin_op('{"a": 1}'::bson, ARRAY['{"a": 2}'::bson, '{"a": 3}'::bson]);
SELECT bson_all_op('["a", "b", "c"]'::bson, ARRAY['a', 'b']);
SELECT bson_size_op('["a", "b", "c"]'::bson, 3);

-- Test type checking
SELECT bson_type_op('{"a": 1}'::bson, 3);  -- Object type
SELECT bson_type_op('["a", "b"]'::bson, 4); -- Array type
SELECT bson_type_op('"hello"'::bson, 2);    -- String type
SELECT bson_type_op('123'::bson, 1);        -- Double type

-- Test regex operator
SELECT bson_regex_op('"hello world"'::bson, 'hello');
SELECT bson_regex_op('"test"'::bson, '^test$');

-- Test modulo operator
SELECT bson_mod_op('{"a": 7}'::bson, 3, 1);
SELECT bson_mod_op('{"a": 10}'::bson, 3, 1);

-- Test element match
SELECT bson_elem_match_op('{"users": [{"name": "john", "age": 30}]}'::bson, '{"name": "john"}'::bson);

-- Test exists operator
SELECT bson_exists_op('{"a": 1, "b": 2}'::bson, 'a');
SELECT bson_exists_op('{"a": 1, "b": 2}'::bson, 'c');
