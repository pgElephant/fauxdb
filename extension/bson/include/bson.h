/*
 * bson.h
 *
 * PostgreSQL BSON extension - main header file
 *
 * This header defines the main BSON data type and core functions
 * for the PostgreSQL BSON extension.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    contrib/bson/bson.h
 */

#ifndef BSON_H
#define BSON_H

#include "postgres.h"
#include "fmgr.h"

/*
 * BSON type constants - matching MongoDB's type codes exactly
 */
#define BSON_TYPE_EOO		0x00	/* End of Object */
#define BSON_TYPE_DOUBLE	0x01	/* Double (64-bit floating point) */
#define BSON_TYPE_STRING	0x02	/* String (UTF-8) */
#define BSON_TYPE_OBJECT	0x03	/* Object (embedded document) */
#define BSON_TYPE_ARRAY		0x04	/* Array (list of values) */
#define BSON_TYPE_BINARY	0x05	/* Binary data */
#define BSON_TYPE_UNDEFINED	0x06	/* Undefined (deprecated) */
#define BSON_TYPE_OBJECTID	0x07	/* ObjectId (12-byte unique ID) */
#define BSON_TYPE_BOOL		0x08	/* Boolean */
#define BSON_TYPE_DATE		0x09	/* Date (millisecond precision UTC datetime) */
#define BSON_TYPE_NULL		0x0A	/* Null */
#define BSON_TYPE_REGEX		0x0B	/* Regular Expression */
#define BSON_TYPE_DBPOINTER	0x0C	/* DBPointer (deprecated) */
#define BSON_TYPE_JAVASCRIPT	0x0D	/* JavaScript code */
#define BSON_TYPE_SYMBOL	0x0E	/* Symbol (deprecated) */
#define BSON_TYPE_JAVASCRIPT_WITH_SCOPE 0x0F	/* JavaScript (with scope) */
#define BSON_TYPE_INT32		0x10	/* 32-bit Integer */
#define BSON_TYPE_TIMESTAMP	0x11	/* Timestamp (special internal type) */
#define BSON_TYPE_INT64		0x12	/* 64-bit Integer */
#define BSON_TYPE_DECIMAL128	0x13	/* Decimal128 (high-precision decimal) */
#define BSON_TYPE_MINKEY	0xFF	/* MinKey (sorts lower than all others) */
#define BSON_TYPE_MAXKEY	0x7F	/* MaxKey (sorts higher than all others) */

/*
 * Main BSON value structure
 */
typedef struct BsonValue
{
	uint8		type;			/* BSON type */
	union
	{
		double		double_val;
		int32		int32_val;
		int64		int64_val;
		bool		bool_val;
		char	   *string_val;
		void	   *object;		/* Object members */
		void	   *array;		/* Array elements */
	}			data;
} BsonValue;

/*
 * Core BSON functions
 */
extern Datum bson_in(PG_FUNCTION_ARGS);
extern Datum bson_out(PG_FUNCTION_ARGS);
extern Datum bson_recv(PG_FUNCTION_ARGS);
extern Datum bson_send(PG_FUNCTION_ARGS);
extern Datum bson_get(PG_FUNCTION_ARGS);
extern Datum bson_get_text(PG_FUNCTION_ARGS);
extern Datum bson_exists(PG_FUNCTION_ARGS);
extern Datum bson_exists_any(PG_FUNCTION_ARGS);
extern Datum bson_exists_all(PG_FUNCTION_ARGS);
extern Datum bson_contains(PG_FUNCTION_ARGS);
extern Datum bson_contained(PG_FUNCTION_ARGS);
extern Datum bson_eq(PG_FUNCTION_ARGS);
extern Datum bson_ne(PG_FUNCTION_ARGS);
extern Datum bson_gt(PG_FUNCTION_ARGS);
extern Datum bson_lt(PG_FUNCTION_ARGS);
extern Datum bson_gte(PG_FUNCTION_ARGS);
extern Datum bson_lte(PG_FUNCTION_ARGS);
extern Datum bson_hash(PG_FUNCTION_ARGS);
extern Datum bson_compare(PG_FUNCTION_ARGS);

/*
 * BSON operator functions
 */
extern Datum bson_eq_op(PG_FUNCTION_ARGS);
extern Datum bson_ne_op(PG_FUNCTION_ARGS);
extern Datum bson_gt_op(PG_FUNCTION_ARGS);
extern Datum bson_lt_op(PG_FUNCTION_ARGS);
extern Datum bson_gte_op(PG_FUNCTION_ARGS);
extern Datum bson_lte_op(PG_FUNCTION_ARGS);
extern Datum bson_in_op(PG_FUNCTION_ARGS);
extern Datum bson_nin_op(PG_FUNCTION_ARGS);
extern Datum bson_all_op(PG_FUNCTION_ARGS);
extern Datum bson_elem_match_op(PG_FUNCTION_ARGS);
extern Datum bson_size_op(PG_FUNCTION_ARGS);
extern Datum bson_type_op(PG_FUNCTION_ARGS);
extern Datum bson_mod_op(PG_FUNCTION_ARGS);
extern Datum bson_regex_op(PG_FUNCTION_ARGS);
extern Datum bson_exists_op(PG_FUNCTION_ARGS);

/*
 * BSON type OID (will be set at runtime)
 */
extern Oid BSONOID;

#endif							/* BSON_H */
