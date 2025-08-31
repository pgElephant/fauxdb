#ifndef BSON_OBJECTID_H
#define BSON_OBJECTID_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/uuid.h"

/* ObjectId - 12-byte unique identifier (MongoDB's _id) */
typedef struct ObjectId
{
	char		data[12];		/* 12-byte ObjectId */
} ObjectId;

/* ObjectId functions for PostgreSQL type system */
extern Datum objectid_in(PG_FUNCTION_ARGS);
extern Datum objectid_out(PG_FUNCTION_ARGS);
extern Datum objectid_recv(PG_FUNCTION_ARGS);
extern Datum objectid_send(PG_FUNCTION_ARGS);

/* Comparison operators */
extern Datum objectid_eq(PG_FUNCTION_ARGS);
extern Datum objectid_ne(PG_FUNCTION_ARGS);
extern Datum objectid_lt(PG_FUNCTION_ARGS);
extern Datum objectid_gt(PG_FUNCTION_ARGS);
extern Datum objectid_le(PG_FUNCTION_ARGS);
extern Datum objectid_ge(PG_FUNCTION_ARGS);

/* Hash function for indexing */
extern Datum objectid_hash(PG_FUNCTION_ARGS);

/* Utility functions */
extern ObjectId *objectid_generate(void);
extern char *objectid_to_string(ObjectId *oid);
extern ObjectId *string_to_objectid(const char *str);
extern bool objectid_is_valid(const char *str);

#endif							/* BSON_OBJECTID_H */
