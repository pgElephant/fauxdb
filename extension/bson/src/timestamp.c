/*
 * timestamp.c
 *
 * PostgreSQL BSON extension - BsonTimestamp type support
 *
 * This module implements BsonTimestamp type support for the BSON extension.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    contrib/bson/timestamp.c
 */

#include "postgres.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fmgr.h"
#include "utils/builtins.h"

#include "../include/timestamp.h"

/*
 * Input function for BsonTimestamp type
 */
PG_FUNCTION_INFO_V1(bson_timestamp_in);

Datum
bson_timestamp_in(PG_FUNCTION_ARGS)
{
	char		   *str = PG_GETARG_CSTRING(0);
	BsonTimestamp  *ts = (BsonTimestamp *) palloc(sizeof(BsonTimestamp));

	sscanf(str, "%lld:%d", (long long *) &ts->seconds, &ts->increment);

	PG_RETURN_POINTER(ts);
}

/*
 * Output function for BsonTimestamp type
 */
PG_FUNCTION_INFO_V1(bson_timestamp_out);

Datum
bson_timestamp_out(PG_FUNCTION_ARGS)
{
	BsonTimestamp  *ts = (BsonTimestamp *) PG_GETARG_POINTER(0);
	char		   *str = (char *) palloc(32);

	snprintf(str, 32, "%lld:%d", (long long) ts->seconds, ts->increment);

	PG_RETURN_CSTRING(str);
}

/*
 * Equality operator for BsonTimestamp type
 */
PG_FUNCTION_INFO_V1(bson_timestamp_eq);

Datum
bson_timestamp_eq(PG_FUNCTION_ARGS)
{
	BsonTimestamp  *a = (BsonTimestamp *) PG_GETARG_POINTER(0);
	BsonTimestamp  *b = (BsonTimestamp *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(a->seconds == b->seconds && a->increment == b->increment);
}

/*
 * Inequality operator for BsonTimestamp type
 */
PG_FUNCTION_INFO_V1(bson_timestamp_ne);

Datum
bson_timestamp_ne(PG_FUNCTION_ARGS)
{
	BsonTimestamp  *a = (BsonTimestamp *) PG_GETARG_POINTER(0);
	BsonTimestamp  *b = (BsonTimestamp *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(a->seconds != b->seconds || a->increment != b->increment);
}

/*
 * Less than operator for BsonTimestamp type
 */
PG_FUNCTION_INFO_V1(bson_timestamp_lt);

Datum
bson_timestamp_lt(PG_FUNCTION_ARGS)
{
	BsonTimestamp  *a = (BsonTimestamp *) PG_GETARG_POINTER(0);
	BsonTimestamp  *b = (BsonTimestamp *) PG_GETARG_POINTER(1);

	if (a->seconds < b->seconds)
		PG_RETURN_BOOL(true);
	if (a->seconds == b->seconds && a->increment < b->increment)
		PG_RETURN_BOOL(true);

	PG_RETURN_BOOL(false);
}

/*
 * Greater than operator for BsonTimestamp type
 */
PG_FUNCTION_INFO_V1(bson_timestamp_gt);

Datum
bson_timestamp_gt(PG_FUNCTION_ARGS)
{
	BsonTimestamp  *a = (BsonTimestamp *) PG_GETARG_POINTER(0);
	BsonTimestamp  *b = (BsonTimestamp *) PG_GETARG_POINTER(1);

	if (a->seconds > b->seconds)
		PG_RETURN_BOOL(true);
	if (a->seconds == b->seconds && a->increment > b->increment)
		PG_RETURN_BOOL(true);

	PG_RETURN_BOOL(false);
}

/*
 * Less than or equal operator for BsonTimestamp type
 */
PG_FUNCTION_INFO_V1(bson_timestamp_le);

Datum
bson_timestamp_le(PG_FUNCTION_ARGS)
{
	BsonTimestamp  *a = (BsonTimestamp *) PG_GETARG_POINTER(0);
	BsonTimestamp  *b = (BsonTimestamp *) PG_GETARG_POINTER(1);

	if (a->seconds < b->seconds)
		PG_RETURN_BOOL(true);
	if (a->seconds == b->seconds && a->increment <= b->increment)
		PG_RETURN_BOOL(true);

	PG_RETURN_BOOL(false);
}

/*
 * Greater than or equal operator for BsonTimestamp type
 */
PG_FUNCTION_INFO_V1(bson_timestamp_ge);

Datum
bson_timestamp_ge(PG_FUNCTION_ARGS)
{
	BsonTimestamp  *a = (BsonTimestamp *) PG_GETARG_POINTER(0);
	BsonTimestamp  *b = (BsonTimestamp *) PG_GETARG_POINTER(1);

	if (a->seconds > b->seconds)
		PG_RETURN_BOOL(true);
	if (a->seconds == b->seconds && a->increment >= b->increment)
		PG_RETURN_BOOL(true);

	PG_RETURN_BOOL(false);
}

/*
 * Hash function for BsonTimestamp type
 */
PG_FUNCTION_INFO_V1(bson_timestamp_hash);

Datum
bson_timestamp_hash(PG_FUNCTION_ARGS)
{
	BsonTimestamp  *ts = (BsonTimestamp *) PG_GETARG_POINTER(0);
	uint32			hash = (uint32) ts->seconds ^ (uint32) ts->increment;

	PG_RETURN_INT32((int32) hash);
}

/*
 * Create BsonTimestamp instance
 */
BsonTimestamp *
bson_timestamp_create(int64 seconds, int32 increment)
{
	BsonTimestamp  *ts = (BsonTimestamp *) palloc(sizeof(BsonTimestamp));

	ts->seconds = seconds;
	ts->increment = increment;

	return ts;
}

/*
 * Create BsonTimestamp for current time
 */
BsonTimestamp *
bson_timestamp_now(void)
{
	BsonTimestamp  *ts = (BsonTimestamp *) palloc(sizeof(BsonTimestamp));

	ts->seconds = (int64) time(NULL);
	ts->increment = 1;

	return ts;
}

/*
 * Get seconds from BsonTimestamp
 */
int64
bson_timestamp_get_seconds(BsonTimestamp *ts)
{
	return ts->seconds;
}

/*
 * Get increment from BsonTimestamp
 */
int32
bson_timestamp_get_increment(BsonTimestamp *ts)
{
	return ts->increment;
}

/*
 * Convert BsonTimestamp to string
 */
char *
bson_timestamp_to_string(BsonTimestamp *ts)
{
	char	   *str = (char *) palloc(32);

	snprintf(str, 32, "%lld:%d", (long long) ts->seconds, ts->increment);

	return str;
}
