/*
 * bson.c
 *
 * PostgreSQL BSON extension using mongo-c-driver
 *
 * This module implements a PostgreSQL data type that stores and manipulates
 * BSON (Binary JSON) data using the official MongoDB C driver.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    contrib/bson/bson.c
 */

#include "postgres.h"

#include <string.h>

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/varlena.h"

/* mongo-c-driver includes */
#include <bson/bson.h>

PG_MODULE_MAGIC;

/*
 * PostgreSQL BSON storage structure
 * We store the BSON data in its native binary format from mongo-c-driver
 */
typedef struct
{
	int32		vl_len_;		/* varlena header */
	char		data[FLEXIBLE_ARRAY_MEMBER];	/* BSON binary data */
} BsonData;

#define DatumGetBsonP(X)		((BsonData *) PG_DETOAST_DATUM(X))
#define BsonPGetDatum(X)		PointerGetDatum(X)
#define PG_GETARG_BSON_P(n)		DatumGetBsonP(PG_GETARG_DATUM(n))
#define PG_RETURN_BSON_P(x)		PG_RETURN_POINTER(x)

/*
 * Forward declarations
 */
extern Datum bson_in(PG_FUNCTION_ARGS);
extern Datum bson_out(PG_FUNCTION_ARGS);
extern Datum bson_recv(PG_FUNCTION_ARGS);
extern Datum bson_send(PG_FUNCTION_ARGS);
extern Datum bson_get(PG_FUNCTION_ARGS);
extern Datum bson_get_text(PG_FUNCTION_ARGS);
extern Datum bson_exists(PG_FUNCTION_ARGS);
extern Datum bson_eq(PG_FUNCTION_ARGS);
extern Datum bson_ne(PG_FUNCTION_ARGS);

/*
 * Helper function to create BsonData from bson_t
 */
static BsonData *
bson_t_to_bson_data(const bson_t *bson)
{
	BsonData   *result;
	const uint8_t *data;
	uint32_t	len;

	data = bson_get_data(bson);
	len = bson->len;

	result = (BsonData *) palloc(VARHDRSZ + len);
	SET_VARSIZE(result, VARHDRSZ + len);
	memcpy(result->data, data, len);

	return result;
}

/*
 * Helper function to create bson_t from BsonData
 */
static bson_t *
bson_data_to_bson_t(const BsonData *bson_data)
{
	const uint8_t *data = (const uint8_t *) bson_data->data;
	size_t		len = VARSIZE_ANY_EXHDR(bson_data);

	return bson_new_from_data(data, len);
}

/*
 * Input function for BSON type
 * Converts JSON string to BSON
 */
PG_FUNCTION_INFO_V1(bson_in);

Datum
bson_in(PG_FUNCTION_ARGS)
{
	char	   *input = PG_GETARG_CSTRING(0);
	bson_t	   *bson;
	bson_error_t error;
	BsonData   *result;

	/* Parse JSON string using mongo-c-driver */
	bson = bson_new_from_json((const uint8_t *) input, -1, &error);
	if (!bson)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type bson: \"%s\"", input),
				 errdetail("%s", error.message)));
	}

	/* Convert to PostgreSQL storage format */
	result = bson_t_to_bson_data(bson);
	bson_destroy(bson);

	PG_RETURN_POINTER(result);
}

/*
 * Output function for BSON type
 * Converts BSON to JSON string
 */
PG_FUNCTION_INFO_V1(bson_out);

Datum
bson_out(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	bson_t	   *bson;
	char	   *json_str;
	char	   *result;

	/* Convert to bson_t */
	bson = bson_data_to_bson_t(bson_data);
	if (!bson)
		ereport(ERROR, (errmsg("invalid BSON data")));

	/* Convert to JSON string */
	json_str = bson_as_canonical_extended_json(bson, NULL);
	result = pstrdup(json_str);

	bson_free(json_str);
	bson_destroy(bson);

	PG_RETURN_CSTRING(result);
}

/*
 * Receive function for BSON type (binary input)
 */
PG_FUNCTION_INFO_V1(bson_recv);

Datum
bson_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	BsonData   *result;
	int32		len;
	const char *data;

	/* Read length and data from binary representation */
	len = pq_getmsgint(buf, 4);
	data = pq_getmsgbytes(buf, len);

	/* Create BsonData structure */
	result = (BsonData *) palloc(VARHDRSZ + len);
	SET_VARSIZE(result, VARHDRSZ + len);
	memcpy(result->data, data, len);

	PG_RETURN_POINTER(result);
}

/*
 * Send function for BSON type (binary output)
 */
PG_FUNCTION_INFO_V1(bson_send);

Datum
bson_send(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	StringInfo	buf;
	int32		len;

	buf = makeStringInfo();

	len = VARSIZE_ANY_EXHDR(bson_data);
	pq_sendint32(buf, len);
	pq_sendbytes(buf, bson_data->data, len);

	PG_RETURN_BYTEA_P(pq_endtypsend(buf));
}

/*
 * Get element (-> operator)
 * Returns BSON value for the given key
 */
PG_FUNCTION_INFO_V1(bson_get);

Datum
bson_get(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	text	   *key = PG_GETARG_TEXT_P(1);
	char	   *key_str;
	bson_t	   *bson;
	bson_iter_t iter;
	const bson_value_t *value;
	bson_t	   *result_bson;
	BsonData   *result;

	key_str = text_to_cstring(key);
	bson = bson_data_to_bson_t(bson_data);

	if (!bson || !bson_iter_init_find(&iter, bson, key_str))
	{
		bson_destroy(bson);
		pfree(key_str);
		PG_RETURN_NULL();
	}

	/* Get the value and create a new BSON document with just that value */
	value = bson_iter_value(&iter);
	result_bson = bson_new();
	bson_append_value(result_bson, "value", -1, value);

	result = bson_t_to_bson_data(result_bson);

	bson_destroy(bson);
	bson_destroy(result_bson);
	pfree(key_str);

	PG_RETURN_POINTER(result);
}

/*
 * Get element as text (->> operator)
 * Returns text representation of the value for the given key
 */
PG_FUNCTION_INFO_V1(bson_get_text);

Datum
bson_get_text(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	text	   *key = PG_GETARG_TEXT_P(1);
	char	   *key_str;
	bson_t	   *bson;
	bson_iter_t iter;
	const bson_value_t *value;
	char	   *result_str = NULL;
	text	   *result;

	key_str = text_to_cstring(key);
	bson = bson_data_to_bson_t(bson_data);

	if (!bson || !bson_iter_init_find(&iter, bson, key_str))
	{
		bson_destroy(bson);
		pfree(key_str);
		PG_RETURN_NULL();
	}

	value = bson_iter_value(&iter);

	/* Convert value to string based on type */
	switch (value->value_type)
	{
		case BSON_TYPE_UTF8:
			result_str = pstrdup(value->value.v_utf8.str);
			break;
		case BSON_TYPE_INT32:
			result_str = psprintf("%d", value->value.v_int32);
			break;
		case BSON_TYPE_INT64:
			result_str = psprintf("%lld", (long long) value->value.v_int64);
			break;
		case BSON_TYPE_DOUBLE:
			result_str = psprintf("%g", value->value.v_double);
			break;
		case BSON_TYPE_BOOL:
			result_str = pstrdup(value->value.v_bool ? "true" : "false");
			break;
		case BSON_TYPE_NULL:
			result_str = pstrdup("null");
			break;
		default:
			result_str = pstrdup("");
			break;
	}

	result = cstring_to_text(result_str);

	bson_destroy(bson);
	pfree(key_str);
	pfree(result_str);

	PG_RETURN_TEXT_P(result);
}

/*
 * Check if key exists (? operator)
 */
PG_FUNCTION_INFO_V1(bson_exists);

Datum
bson_exists(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	text	   *key = PG_GETARG_TEXT_P(1);
	char	   *key_str;
	bson_t	   *bson;
	bool		exists;

	key_str = text_to_cstring(key);
	bson = bson_data_to_bson_t(bson_data);

	exists = bson && bson_has_field(bson, key_str);

	bson_destroy(bson);
	pfree(key_str);

	PG_RETURN_BOOL(exists);
}

/*
 * Equality operator (=)
 */
PG_FUNCTION_INFO_V1(bson_eq);

Datum
bson_eq(PG_FUNCTION_ARGS)
{
	BsonData   *bson1 = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *bson2 = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *b1, *b2;
	bool		result;

	b1 = bson_data_to_bson_t(bson1);
	b2 = bson_data_to_bson_t(bson2);

	result = (b1 && b2 && bson_equal(b1, b2));

	bson_destroy(b1);
	bson_destroy(b2);

	PG_RETURN_BOOL(result);
}

/*
 * Inequality operator (!=, <>)
 */
PG_FUNCTION_INFO_V1(bson_ne);

Datum
bson_ne(PG_FUNCTION_ARGS)
{
	BsonData   *bson1 = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *bson2 = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *b1, *b2;
	bool		result;

	b1 = bson_data_to_bson_t(bson1);
	b2 = bson_data_to_bson_t(bson2);

	result = !(b1 && b2 && bson_equal(b1, b2));

	bson_destroy(b1);
	bson_destroy(b2);

	PG_RETURN_BOOL(result);
}
