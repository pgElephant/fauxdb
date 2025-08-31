#ifndef BSON_TIMESTAMP_H
#define BSON_TIMESTAMP_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/timestamp.h"

/* BsonTimestamp - internal MongoDB timestamp */
typedef struct BsonTimestamp
{
	int64		seconds;		/* Seconds since epoch */
	int32		increment;		/* Increment counter */
} BsonTimestamp;

/* BsonTimestamp functions for PostgreSQL type system */
extern Datum bson_timestamp_in(PG_FUNCTION_ARGS);
extern Datum bson_timestamp_out(PG_FUNCTION_ARGS);
extern Datum bson_timestamp_recv(PG_FUNCTION_ARGS);
extern Datum bson_timestamp_send(PG_FUNCTION_ARGS);

/* Comparison operators */
extern Datum bson_timestamp_eq(PG_FUNCTION_ARGS);
extern Datum bson_timestamp_ne(PG_FUNCTION_ARGS);
extern Datum bson_timestamp_lt(PG_FUNCTION_ARGS);
extern Datum bson_timestamp_gt(PG_FUNCTION_ARGS);
extern Datum bson_timestamp_le(PG_FUNCTION_ARGS);
extern Datum bson_timestamp_ge(PG_FUNCTION_ARGS);

/* Hash function for indexing */
extern Datum bson_timestamp_hash(PG_FUNCTION_ARGS);

/* Utility functions */
extern BsonTimestamp *bson_timestamp_create(int64 seconds, int32 increment);
extern BsonTimestamp *bson_timestamp_now(void);
extern int64 bson_timestamp_get_seconds(BsonTimestamp *ts);
extern int32 bson_timestamp_get_increment(BsonTimestamp *ts);
extern BsonTimestamp *bson_timestamp_from_postgres_timestamp(TimestampTz pg_ts);
extern TimestampTz bson_timestamp_to_postgres_timestamp(BsonTimestamp *ts);
extern char *bson_timestamp_to_string(BsonTimestamp *ts);

#endif							/* BSON_TIMESTAMP_H */
