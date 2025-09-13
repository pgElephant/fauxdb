/*
 * bson.h
 *		  PostgreSQL BSON extension - main header file
 *
 * This header defines the main BSON data type and core functions
 * for the PostgreSQL BSON extension, providing compatibility with
 * the standard BSON format.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  extension/bson/include/bson.h
 */

#ifndef BSON_H
#define BSON_H

#include "fmgr.h"

/*
 * BSON type constants - matching standard BSON type codes exactly
 *
 * These constants define the binary format identifiers used in
 * BSON documents, ensuring full compatibility with the BSON standard.
 */
#define BSON_TYPE_EOO 0x00                   /* End of Object */
#define BSON_TYPE_DOUBLE 0x01                /* 64-bit floating point */
#define BSON_TYPE_STRING 0x02                /* UTF-8 string */
#define BSON_TYPE_OBJECT 0x03                /* Embedded document */
#define BSON_TYPE_ARRAY 0x04                 /* Array of values */
#define BSON_TYPE_BINARY 0x05                /* Binary data */
#define BSON_TYPE_UNDEFINED 0x06             /* Undefined (deprecated) */
#define BSON_TYPE_OBJECTID 0x07              /* 12-byte unique ID */
#define BSON_TYPE_BOOL 0x08                  /* Boolean */
#define BSON_TYPE_DATE 0x09                  /* UTC datetime (ms precision) */
#define BSON_TYPE_NULL 0x0A                  /* Null */
#define BSON_TYPE_REGEX 0x0B                 /* Regular expression */
#define BSON_TYPE_DBPOINTER 0x0C             /* DBPointer (deprecated) */
#define BSON_TYPE_JAVASCRIPT 0x0D            /* JavaScript code */
#define BSON_TYPE_SYMBOL 0x0E                /* Symbol (deprecated) */
#define BSON_TYPE_JAVASCRIPT_WITH_SCOPE 0x0F /* JavaScript with scope */
#define BSON_TYPE_INT32 0x10                 /* 32-bit integer */
#define BSON_TYPE_TIMESTAMP 0x11             /* Special internal type */
#define BSON_TYPE_INT64 0x12                 /* 64-bit integer */
#define BSON_TYPE_DECIMAL128 0x13            /* High-precision decimal */
#define BSON_TYPE_MINKEY 0xFF                /* Sorts lower than all */
#define BSON_TYPE_MAXKEY 0x7F                /* Sorts higher than all */

/*
 * Core BSON function declarations
 *
 * These functions implement the essential BSON type operations
 * required by PostgreSQL's type system.
 */

/* Input/output functions */
extern Datum bson_in(PG_FUNCTION_ARGS);
extern Datum bson_out(PG_FUNCTION_ARGS);
extern Datum bson_recv(PG_FUNCTION_ARGS);
extern Datum bson_send(PG_FUNCTION_ARGS);

/* Access functions */
extern Datum bson_get(PG_FUNCTION_ARGS);
extern Datum bson_get_text(PG_FUNCTION_ARGS);

/* Existence check functions */
extern Datum bson_exists(PG_FUNCTION_ARGS);
extern Datum bson_exists_any(PG_FUNCTION_ARGS);
extern Datum bson_exists_all(PG_FUNCTION_ARGS);

/* Containment functions */
extern Datum bson_contains(PG_FUNCTION_ARGS);
extern Datum bson_contained(PG_FUNCTION_ARGS);

/* Comparison functions */
extern Datum bson_eq(PG_FUNCTION_ARGS);
extern Datum bson_ne(PG_FUNCTION_ARGS);
extern Datum bson_gt(PG_FUNCTION_ARGS);
extern Datum bson_lt(PG_FUNCTION_ARGS);
extern Datum bson_ge(PG_FUNCTION_ARGS);
extern Datum bson_le(PG_FUNCTION_ARGS);
extern Datum bson_cmp(PG_FUNCTION_ARGS);

/* Hash function */
extern Datum bson_hash(PG_FUNCTION_ARGS);

#endif /* BSON_H */
