-- BSON extension for MongoDB-compatible BSON processing
-- This extension provides a bson type that mimics MongoDB's BSON format

-- Basic I/O functions
CREATE FUNCTION bson_in(cstring) RETURNS bson
    AS '$libdir/bson', 'bson_in'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_out(bson) RETURNS cstring
    AS '$libdir/bson', 'bson_out'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_recv(internal) RETURNS bson
    AS '$libdir/bson', 'bson_recv'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_send(bson) RETURNS bytea
    AS '$libdir/bson', 'bson_send'
    LANGUAGE C IMMUTABLE STRICT;

-- Create the bson type with all properties
CREATE TYPE bson (
    internallength = variable,
    input = bson_in,
    output = bson_out,
    receive = bson_recv,
    send = bson_send,
    alignment = double
);

-- Element access functions
CREATE FUNCTION bson_get(bson, text) RETURNS bson
    AS '$libdir/bson', 'bson_get'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_get_text(bson, text) RETURNS text
    AS '$libdir/bson', 'bson_get_text'
    LANGUAGE C IMMUTABLE STRICT;

-- Existence check functions
CREATE FUNCTION bson_exists(bson, text) RETURNS boolean
    AS '$libdir/bson', 'bson_exists'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_exists_any(bson, text[]) RETURNS boolean
    AS '$libdir/bson', 'bson_exists_any'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_exists_all(bson, text[]) RETURNS boolean
    AS '$libdir/bson', 'bson_exists_all'
    LANGUAGE C IMMUTABLE STRICT;

-- Containment functions
CREATE FUNCTION bson_contains(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_contains'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_contained(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_contained'
    LANGUAGE C IMMUTABLE STRICT;

-- Comparison functions
CREATE FUNCTION bson_eq(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_eq'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_ne(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_ne'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_gt(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_gt'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_lt(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_lt'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_gte(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_gte'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_lte(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_lte'
    LANGUAGE C IMMUTABLE STRICT;

-- Hash and comparison functions
CREATE FUNCTION bson_hash(bson) RETURNS integer
    AS '$libdir/bson', 'bson_hash'
    LANGUAGE C IMMUTABLE STRICT;

-- Comparison functions
CREATE FUNCTION bson_cmp(bson, bson) RETURNS integer
    AS '$libdir/bson', 'bson_cmp'
    LANGUAGE C IMMUTABLE STRICT;

-- BSON Query Operator functions
CREATE FUNCTION bson_eq_op(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_eq_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_ne_op(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_ne_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_gt_op(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_gt_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_lt_op(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_lt_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_gte_op(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_gte_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_lte_op(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_lte_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_in_op(bson, bson[]) RETURNS boolean
    AS '$libdir/bson', 'bson_in_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_nin_op(bson, bson[]) RETURNS boolean
    AS '$libdir/bson', 'bson_nin_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_all_op(bson, text[]) RETURNS boolean
    AS '$libdir/bson', 'bson_all_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_size_op(bson, integer) RETURNS boolean
    AS '$libdir/bson', 'bson_size_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_type_op(bson, integer) RETURNS boolean
    AS '$libdir/bson', 'bson_type_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_regex_op(bson, text) RETURNS boolean
    AS '$libdir/bson', 'bson_regex_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_exists_op(bson, text) RETURNS boolean
    AS '$libdir/bson', 'bson_exists_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_mod_op(bson, integer, integer) RETURNS boolean
    AS '$libdir/bson', 'bson_mod_op'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_elem_match_op(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_elem_match_op'
    LANGUAGE C IMMUTABLE STRICT;

-- Create operators
CREATE OPERATOR -> (
    LEFTARG = bson,
    RIGHTARG = text,
    PROCEDURE = bson_get
);

CREATE OPERATOR ->> (
    LEFTARG = bson,
    RIGHTARG = text,
    PROCEDURE = bson_get_text
);

CREATE OPERATOR = (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_eq,
    COMMUTATOR = =,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR != (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_ne,
    COMMUTATOR = !=,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

CREATE OPERATOR > (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_gt,
    COMMUTATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR < (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_lt,
    COMMUTATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_gte,
    COMMUTATOR = <=,
    RESTRICT = scalargesel,
    JOIN = scalargejoinsel
);

CREATE OPERATOR <= (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_lte,
    COMMUTATOR = >=,
    RESTRICT = scalarlesel,
    JOIN = scalarlejoinsel
);

CREATE OPERATOR @> (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_contains,
    RESTRICT = contsel,
    JOIN = contjoinsel
);

CREATE OPERATOR <@ (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_contained,
    RESTRICT = contsel,
    JOIN = contjoinsel
);

-- Existence operators
CREATE OPERATOR ? (
    LEFTARG = bson,
    RIGHTARG = text,
    PROCEDURE = bson_exists
);

CREATE OPERATOR ?| (
    LEFTARG = bson,
    RIGHTARG = text[],
    PROCEDURE = bson_exists_any
);

CREATE OPERATOR ?& (
    LEFTARG = bson,
    RIGHTARG = text[],
    PROCEDURE = bson_exists_all
);

-- BSON Query Operator functions (for MongoDB-style queries)
-- These are not operators but functions that can be used in WHERE clauses
-- Example: WHERE bson_eq_op(data, '{"field": "value"}')

-- Create operator classes for indexing
CREATE OPERATOR CLASS bson_ops
    DEFAULT FOR TYPE bson USING btree AS
        OPERATOR    1   <,
        OPERATOR    2   <=,
        OPERATOR    3   =,
        OPERATOR    4   >=,
        OPERATOR    5   >,
        FUNCTION    1   bson_cmp(bson, bson);

CREATE OPERATOR CLASS bson_hash_ops
    DEFAULT FOR TYPE bson USING hash AS
        OPERATOR    1   =,
        FUNCTION    1   bson_hash(bson);

-- Grant permissions
GRANT USAGE ON TYPE bson TO PUBLIC;
