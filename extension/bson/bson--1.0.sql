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
    AS '$libdir/bson', 'bson_ge'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_lte(bson, bson) RETURNS boolean
    AS '$libdir/bson', 'bson_le'
    LANGUAGE C IMMUTABLE STRICT;

-- Hash and comparison functions
CREATE FUNCTION bson_hash(bson) RETURNS integer
    AS '$libdir/bson', 'bson_hash'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION bson_cmp(bson, bson) RETURNS integer
    AS '$libdir/bson', 'bson_cmp'
    LANGUAGE C IMMUTABLE STRICT;

-- Document merge function
CREATE FUNCTION bson_merge(bson, bson) RETURNS bson
    AS '$libdir/bson', 'bson_merge'
    LANGUAGE C IMMUTABLE STRICT;

-- Standard PostgreSQL operators
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

-- MongoDB-style operators (alternative syntax)
CREATE OPERATOR ~= (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_eq,
    COMMUTATOR = ~=,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR ~> (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_contains,
    RESTRICT = contsel,
    JOIN = contjoinsel
);

CREATE OPERATOR <~ (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_contained,
    RESTRICT = contsel,
    JOIN = contjoinsel
);

CREATE OPERATOR ?? (
    LEFTARG = bson,
    RIGHTARG = text,
    PROCEDURE = bson_exists
);

CREATE OPERATOR ??| (
    LEFTARG = bson,
    RIGHTARG = text[],
    PROCEDURE = bson_exists_any
);

CREATE OPERATOR ??& (
    LEFTARG = bson,
    RIGHTARG = text[],
    PROCEDURE = bson_exists_all
);

-- Document merge operator
CREATE OPERATOR || (
    LEFTARG = bson,
    RIGHTARG = bson,
    PROCEDURE = bson_merge
);

-- Operator classes for indexing
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
