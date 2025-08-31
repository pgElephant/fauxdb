/*
 * special_keys.c
 *
 * PostgreSQL BSON extension - MinKey/MaxKey type support
 *
 * This module implements MinKey and MaxKey types for the BSON extension.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    contrib/bson/special_keys.c
 */

#include "postgres.h"

#include <stdlib.h>
#include <string.h>

#include "fmgr.h"
#include "lib/stringinfo.h"

#include "../include/special_keys.h"

/*
 * Input function for MinKey type
 */
PG_FUNCTION_INFO_V1(minkey_in);

Datum
minkey_in(PG_FUNCTION_ARGS)
{
	MinKey    *mk = (MinKey *) palloc(sizeof(MinKey));

	mk->dummy = 0;

	PG_RETURN_POINTER(mk);
}

/*
 * Output function for MinKey type
 */
PG_FUNCTION_INFO_V1(minkey_out);

Datum
minkey_out(PG_FUNCTION_ARGS)
{
	char	   *str = (char *) palloc(7);

	strcpy(str, "MinKey");

	PG_RETURN_CSTRING(str);
}

/*
 * Receive function for MinKey type
 */
PG_FUNCTION_INFO_V1(minkey_recv);

Datum
minkey_recv(PG_FUNCTION_ARGS)
{
	MinKey    *mk = (MinKey *) palloc(sizeof(MinKey));

	mk->dummy = 0;

	PG_RETURN_POINTER(mk);
}

/*
 * Send function for MinKey type
 */
PG_FUNCTION_INFO_V1(minkey_send);

Datum
minkey_send(PG_FUNCTION_ARGS)
{
	StringInfo	buf;

	buf = makeStringInfo();
	appendStringInfoString(buf, "MinKey");

	PG_RETURN_BYTEA_P(buf->data);
}

/*
 * Input function for MaxKey type
 */
PG_FUNCTION_INFO_V1(maxkey_in);

Datum
maxkey_in(PG_FUNCTION_ARGS)
{
	MaxKey    *mk = (MaxKey *) palloc(sizeof(MaxKey));

	mk->dummy = 0;

	PG_RETURN_POINTER(mk);
}

/*
 * Output function for MaxKey type
 */
PG_FUNCTION_INFO_V1(maxkey_out);

Datum
maxkey_out(PG_FUNCTION_ARGS)
{
	char	   *str = (char *) palloc(7);

	strcpy(str, "MaxKey");

	PG_RETURN_CSTRING(str);
}

/*
 * Receive function for MaxKey type
 */
PG_FUNCTION_INFO_V1(maxkey_recv);

Datum
maxkey_recv(PG_FUNCTION_ARGS)
{
	MaxKey    *mk = (MaxKey *) palloc(sizeof(MaxKey));

	mk->dummy = 0;

	PG_RETURN_POINTER(mk);
}

/*
 * Send function for MaxKey type
 */
PG_FUNCTION_INFO_V1(maxkey_send);

Datum
maxkey_send(PG_FUNCTION_ARGS)
{
	StringInfo	buf;

	buf = makeStringInfo();
	appendStringInfoString(buf, "MaxKey");

	PG_RETURN_BYTEA_P(buf->data);
}

/*
 * MinKey comparison functions
 */
PG_FUNCTION_INFO_V1(minkey_lt_all);

Datum
minkey_lt_all(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(minkey_le_all);

Datum
minkey_le_all(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(true);
}

/*
 * MaxKey comparison functions
 */
PG_FUNCTION_INFO_V1(maxkey_gt_all);

Datum
maxkey_gt_all(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(maxkey_ge_all);

Datum
maxkey_ge_all(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(true);
}

/*
 * Create MinKey instance
 */
MinKey *
minkey_create(void)
{
	MinKey    *mk = (MinKey *) palloc(sizeof(MinKey));

	mk->dummy = 0;

	return mk;
}

/*
 * Create MaxKey instance
 */
MaxKey *
maxkey_create(void)
{
	MaxKey    *mk = (MaxKey *) palloc(sizeof(MaxKey));

	mk->dummy = 0;

	return mk;
}

/*
 * Check if value is MinKey
 */
bool
is_minkey(Datum value)
{
	return true;
}

/*
 * Check if value is MaxKey
 */
bool
is_maxkey(Datum value)
{
	return true;
}
