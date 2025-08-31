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
#include <ctype.h>

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"
#include "utils/varlena.h"
#include "access/hash.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/varlena.h"
#include "access/hash.h"

/* mongo-c-driver includes */
#include <bson/bson.h>

PG_MODULE_MAGIC;

/*
 * BSON type OID
 */
Oid BSONOID = InvalidOid;

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
extern Datum bson_exists_any(PG_FUNCTION_ARGS);
extern Datum bson_exists_all(PG_FUNCTION_ARGS);
extern Datum bson_contains(PG_FUNCTION_ARGS);
extern Datum bson_contained(PG_FUNCTION_ARGS);
extern Datum bson_eq(PG_FUNCTION_ARGS);
extern Datum bson_ne(PG_FUNCTION_ARGS);
extern Datum bson_gt(PG_FUNCTION_ARGS);
extern Datum bson_gte(PG_FUNCTION_ARGS);
extern Datum bson_lt(PG_FUNCTION_ARGS);
extern Datum bson_lte(PG_FUNCTION_ARGS);
extern Datum bson_cmp(PG_FUNCTION_ARGS);
extern Datum bson_hash(PG_FUNCTION_ARGS);

/*
 * Helper function to compare bson values
 */
static bool
bson_values_equal(const bson_value_t *val1, const bson_value_t *val2)
{
	if (val1->value_type != val2->value_type)
		return false;

	switch (val1->value_type)
	{
		case BSON_TYPE_UTF8:
			return (val1->value.v_utf8.len == val2->value.v_utf8.len &&
					memcmp(val1->value.v_utf8.str, val2->value.v_utf8.str, val1->value.v_utf8.len) == 0);
		case BSON_TYPE_INT32:
			return val1->value.v_int32 == val2->value.v_int32;
		case BSON_TYPE_INT64:
			return val1->value.v_int64 == val2->value.v_int64;
		case BSON_TYPE_DOUBLE:
			return val1->value.v_double == val2->value.v_double;
		case BSON_TYPE_BOOL:
			return val1->value.v_bool == val2->value.v_bool;
		case BSON_TYPE_NULL:
			return true;
		default:
			return false; /* For simplicity, other types not equal */
	}
}

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
	char	   *wrapped_input = NULL;

	/* Try parsing as-is first */
	bson = bson_new_from_json((const uint8_t *) input, -1, &error);
	
	/* If it fails and it's a standalone value, wrap it in a document */
	if (!bson && (input[0] == '"' || isdigit(input[0]) || input[0] == '-' || 
				  strncmp(input, "true", 4) == 0 || strncmp(input, "false", 5) == 0 || 
				  strncmp(input, "null", 4) == 0))
	{
		wrapped_input = psprintf("{\"value\": %s}", input);
		bson = bson_new_from_json((const uint8_t *) wrapped_input, -1, &error);
	}

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
	
	if (wrapped_input)
		pfree(wrapped_input);

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

/*
 * Check if any of the keys exist (?| operator)
 */
PG_FUNCTION_INFO_V1(bson_exists_any);

Datum
bson_exists_any(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	ArrayType  *keys = PG_GETARG_ARRAYTYPE_P(1);
	bson_t	   *bson;
	Datum	   *key_datums;
	bool	   *key_nulls;
	int			key_count;
	bool		found = false;
	int			i;

	bson = bson_data_to_bson_t(bson_data);
	if (!bson)
		PG_RETURN_BOOL(false);

	deconstruct_array(keys, TEXTOID, -1, false, TYPALIGN_INT,
					  &key_datums, &key_nulls, &key_count);

	for (i = 0; i < key_count && !found; i++)
	{
		if (!key_nulls[i])
		{
			char *key_str = TextDatumGetCString(key_datums[i]);
			found = bson_has_field(bson, key_str);
			pfree(key_str);
		}
	}

	bson_destroy(bson);
	pfree(key_datums);
	pfree(key_nulls);

	PG_RETURN_BOOL(found);
}

/*
 * Check if all of the keys exist (?& operator)
 */
PG_FUNCTION_INFO_V1(bson_exists_all);

Datum
bson_exists_all(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	ArrayType  *keys = PG_GETARG_ARRAYTYPE_P(1);
	bson_t	   *bson;
	Datum	   *key_datums;
	bool	   *key_nulls;
	int			key_count;
	bool		all_found = true;
	int			i;

	bson = bson_data_to_bson_t(bson_data);
	if (!bson)
		PG_RETURN_BOOL(false);

	deconstruct_array(keys, TEXTOID, -1, false, TYPALIGN_INT,
					  &key_datums, &key_nulls, &key_count);

	for (i = 0; i < key_count && all_found; i++)
	{
		if (!key_nulls[i])
		{
			char *key_str = TextDatumGetCString(key_datums[i]);
			all_found = bson_has_field(bson, key_str);
			pfree(key_str);
		}
		else
		{
			all_found = false;
		}
	}

	bson_destroy(bson);
	pfree(key_datums);
	pfree(key_nulls);

	PG_RETURN_BOOL(all_found);
}

/*
 * Contains operator (@>)
 */
PG_FUNCTION_INFO_V1(bson_contains);

Datum
bson_contains(PG_FUNCTION_ARGS)
{
	BsonData   *bson1 = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *bson2 = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *b1, *b2;
	bson_iter_t iter1, iter2;
	bool		result = true;

	b1 = bson_data_to_bson_t(bson1);
	b2 = bson_data_to_bson_t(bson2);

	if (!b1 || !b2)
	{
		bson_destroy(b1);
		bson_destroy(b2);
		PG_RETURN_BOOL(false);
	}

	/* Check if all fields in b2 exist in b1 with same values */
	bson_iter_init(&iter2, b2);
	while (bson_iter_next(&iter2) && result)
	{
		const char *key = bson_iter_key(&iter2);
		if (bson_iter_init_find(&iter1, b1, key))
		{
			const bson_value_t *val1 = bson_iter_value(&iter1);
			const bson_value_t *val2 = bson_iter_value(&iter2);
			result = bson_values_equal(val1, val2);
		}
		else
		{
			result = false;
		}
	}

	bson_destroy(b1);
	bson_destroy(b2);

	PG_RETURN_BOOL(result);
}

/*
 * Contained operator (<@)
 */
PG_FUNCTION_INFO_V1(bson_contained);

Datum
bson_contained(PG_FUNCTION_ARGS)
{
	BsonData   *bson1 = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *bson2 = (BsonData *) PG_GETARG_POINTER(1);

	/* bson1 <@ bson2 is equivalent to bson2 @> bson1 */
	return DirectFunctionCall2(bson_contains, PointerGetDatum(bson2), PointerGetDatum(bson1));
}

/*
 * Helper function for BSON comparison
 */
static int
bson_compare_internal(const BsonData *bson1, const BsonData *bson2)
{
	bson_t	   *b1, *b2;
	char	   *json1, *json2;
	int			result;

	b1 = bson_data_to_bson_t(bson1);
	b2 = bson_data_to_bson_t(bson2);

	if (!b1 || !b2)
	{
		bson_destroy(b1);
		bson_destroy(b2);
		return 0;
	}

	/* For simplicity, compare as JSON strings */
	json1 = bson_as_canonical_extended_json(b1, NULL);
	json2 = bson_as_canonical_extended_json(b2, NULL);

	result = strcmp(json1, json2);

	bson_free(json1);
	bson_free(json2);
	bson_destroy(b1);
	bson_destroy(b2);

	return result;
}

/*
 * Compare function for sorting
 */
PG_FUNCTION_INFO_V1(bson_cmp);

Datum
bson_cmp(PG_FUNCTION_ARGS)
{
	BsonData   *bson1 = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *bson2 = (BsonData *) PG_GETARG_POINTER(1);

	PG_RETURN_INT32(bson_compare_internal(bson1, bson2));
}

/*
 * Greater than operator (>)
 */
PG_FUNCTION_INFO_V1(bson_gt);

Datum
bson_gt(PG_FUNCTION_ARGS)
{
	BsonData   *bson1 = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *bson2 = (BsonData *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_internal(bson1, bson2) > 0);
}

/*
 * Greater than or equal operator (>=)
 */
PG_FUNCTION_INFO_V1(bson_gte);

Datum
bson_gte(PG_FUNCTION_ARGS)
{
	BsonData   *bson1 = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *bson2 = (BsonData *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_internal(bson1, bson2) >= 0);
}

/*
 * Less than operator (<)
 */
PG_FUNCTION_INFO_V1(bson_lt);

Datum
bson_lt(PG_FUNCTION_ARGS)
{
	BsonData   *bson1 = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *bson2 = (BsonData *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_internal(bson1, bson2) < 0);
}

/*
 * Less than or equal operator (<=)
 */
PG_FUNCTION_INFO_V1(bson_lte);

Datum
bson_lte(PG_FUNCTION_ARGS)
{
	BsonData   *bson1 = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *bson2 = (BsonData *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_internal(bson1, bson2) <= 0);
}

/*
 * Hash function for hash indexing
 */
PG_FUNCTION_INFO_V1(bson_hash);

Datum
bson_hash(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	uint32		hash;

	/* Hash the raw binary BSON data */
	hash = DatumGetUInt32(DirectFunctionCall2(hashvarlena, 
											  PointerGetDatum(bson_data),
											  Int32GetDatum(0)));

	PG_RETURN_UINT32(hash);
}
