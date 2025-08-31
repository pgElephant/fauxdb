#ifndef BSON_DECIMAL128_H
#define BSON_DECIMAL128_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/numeric.h"

/* Decimal128 - high-precision decimal */
typedef struct Decimal128
{
	char		data[16];		/* 16-byte decimal128 */
} Decimal128;

/* Decimal128 functions for PostgreSQL type system */
extern Datum decimal128_in(PG_FUNCTION_ARGS);
extern Datum decimal128_out(PG_FUNCTION_ARGS);
extern Datum decimal128_recv(PG_FUNCTION_ARGS);
extern Datum decimal128_send(PG_FUNCTION_ARGS);

/* Comparison operators */
extern Datum decimal128_eq(PG_FUNCTION_ARGS);
extern Datum decimal128_ne(PG_FUNCTION_ARGS);
extern Datum decimal128_lt(PG_FUNCTION_ARGS);
extern Datum decimal128_gt(PG_FUNCTION_ARGS);
extern Datum decimal128_le(PG_FUNCTION_ARGS);
extern Datum decimal128_ge(PG_FUNCTION_ARGS);

/* Arithmetic operators */
extern Datum decimal128_add(PG_FUNCTION_ARGS);
extern Datum decimal128_sub(PG_FUNCTION_ARGS);
extern Datum decimal128_mul(PG_FUNCTION_ARGS);
extern Datum decimal128_div(PG_FUNCTION_ARGS);
extern Datum decimal128_mod(PG_FUNCTION_ARGS);

/* Hash function for indexing */
extern Datum decimal128_hash(PG_FUNCTION_ARGS);

/* Utility functions */
extern Decimal128 *decimal128_create(const char *str);
extern char *decimal128_to_string(Decimal128 *dec);
extern Numeric decimal128_to_numeric(Decimal128 *dec);
extern Decimal128 *numeric_to_decimal128(Numeric num);
extern bool decimal128_is_valid(const char *str);

#endif							/* BSON_DECIMAL128_H */
