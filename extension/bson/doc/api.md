# BSON Extension API Reference

## Core Functions

### Input/Output Functions

#### `bson_in(cstring) → bson`
Converts a JSON string to BSON format.

```sql
SELECT '{"name": "test", "value": 42}'::bson;
```

#### `bson_out(bson) → cstring`
Converts BSON to canonical JSON string representation.

```sql
SELECT bson_out('{"name": "test"}'::bson);
```

#### `bson_recv(internal) → bson`
Binary input function for BSON type (internal use).

#### `bson_send(bson) → bytea`
Binary output function for BSON type (internal use).

### Element Access Functions

#### `bson_get(bson, text) → bson`
Gets a BSON element by field name (-> operator).

```sql
SELECT '{"name": "John", "age": 30}'::bson -> 'name';
```

#### `bson_get_text(bson, text) → text`
Gets a BSON element as text (->> operator).

```sql
SELECT '{"name": "John", "age": 30}'::bson ->> 'age';  -- Returns: "30"
```

### Existence Functions

#### `bson_exists(bson, text) → boolean`
Checks if a field exists (? operator).

```sql
SELECT '{"name": "John"}'::bson ? 'name';  -- Returns: true
```

#### `bson_exists_any(bson, text[]) → boolean`
Checks if any of the specified fields exist (?| operator).

```sql
SELECT '{"name": "John"}'::bson ?| ARRAY['name', 'email'];  -- Returns: true
```

#### `bson_exists_all(bson, text[]) → boolean`
Checks if all specified fields exist (?& operator).

```sql
SELECT '{"name": "John"}'::bson ?& ARRAY['name', 'age'];  -- Returns: false
```

### Comparison Functions

#### `bson_eq(bson, bson) → boolean`
Equality comparison (= operator).

#### `bson_ne(bson, bson) → boolean`
Inequality comparison (!= operator).

#### `bson_gt(bson, bson) → boolean`
Greater than comparison (> operator).

#### `bson_lt(bson, bson) → boolean`
Less than comparison (< operator).

#### `bson_gte(bson, bson) → boolean`
Greater than or equal comparison (>= operator).

#### `bson_lte(bson, bson) → boolean`
Less than or equal comparison (<= operator).

### Containment Functions

#### `bson_contains(bson, bson) → boolean`
Checks if left BSON contains right BSON (@> operator).

```sql
SELECT '{"a": 1, "b": 2}'::bson @> '{"a": 1}'::bson;  -- Returns: true
```

#### `bson_contained(bson, bson) → boolean`
Checks if left BSON is contained in right BSON (<@ operator).

```sql
SELECT '{"a": 1}'::bson <@ '{"a": 1, "b": 2}'::bson;  -- Returns: true
```

## MongoDB Query Operators

### Comparison Operators

#### `bson_eq_op(bson, bson) → boolean`
MongoDB `$eq` operator equivalent.

#### `bson_ne_op(bson, bson) → boolean`
MongoDB `$ne` operator equivalent.

#### `bson_gt_op(bson, bson) → boolean`
MongoDB `$gt` operator equivalent.

#### `bson_lt_op(bson, bson) → boolean`
MongoDB `$lt` operator equivalent.

#### `bson_gte_op(bson, bson) → boolean`
MongoDB `$gte` operator equivalent.

#### `bson_lte_op(bson, bson) → boolean`
MongoDB `$lte` operator equivalent.

### Array Operators

#### `bson_in_op(bson, bson[]) → boolean`
MongoDB `$in` operator equivalent.

#### `bson_nin_op(bson, bson[]) → boolean`
MongoDB `$nin` operator equivalent.

#### `bson_all_op(bson, text[]) → boolean`
MongoDB `$all` operator equivalent.

#### `bson_size_op(bson, integer) → boolean`
MongoDB `$size` operator equivalent.

### Element Operators

#### `bson_exists_op(bson, text) → boolean`
MongoDB `$exists` operator equivalent.

#### `bson_type_op(bson, integer) → boolean`
MongoDB `$type` operator equivalent.

### Evaluation Operators

#### `bson_regex_op(bson, text) → boolean`
MongoDB `$regex` operator equivalent.

#### `bson_mod_op(bson, integer, integer) → boolean`
MongoDB `$mod` operator equivalent.

### Array Query Operators

#### `bson_elem_match_op(bson, bson) → boolean`
MongoDB `$elemMatch` operator equivalent.

## Utility Functions

#### `bson_hash(bson) → integer`
Hash function for BSON values (used for hash indexing).

#### `bson_cmp(bson, bson) → integer`
Comparison function for BSON values (used for B-tree indexing).

## Usage Examples

### Complex Queries

```sql
-- Find documents where age is greater than 25
SELECT * FROM users 
WHERE bson_gt_op(data->'age', '25'::bson);

-- Find documents with specific tags
SELECT * FROM posts 
WHERE bson_in_op(data->'tags', ARRAY['postgresql', 'database']::bson[]);

-- Find documents where email field exists
SELECT * FROM users 
WHERE bson_exists_op(data, 'email');

-- Find arrays with exactly 3 elements
SELECT * FROM documents 
WHERE bson_size_op(data->'items', 3);

-- Find string fields matching pattern
SELECT * FROM documents 
WHERE bson_regex_op(data->'title', 'MongoDB.*');
```

### Indexing

```sql
-- B-tree index for general comparisons
CREATE INDEX idx_data_btree ON documents USING btree (data);

-- Hash index for equality lookups
CREATE INDEX idx_data_hash ON documents USING hash (data);

-- Partial index for specific conditions
CREATE INDEX idx_active_users ON users (data) 
WHERE bson_exists_op(data, 'active') AND data->>'active' = 'true';
```
