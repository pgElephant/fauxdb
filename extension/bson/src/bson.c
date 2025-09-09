/*
 * bson.c
 *		  PostgreSQL BSON data type support
 *
 * This module implements a BSON (Binary JSON) data type for PostgreSQL
 * that provides compatibility with the standard BSON format using the
 * official libbson C driver library.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  extension/bson/src/bson.c
 */

#include "postgres.h"

#include "access/hash.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "libpq/pqformat.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

#include <bson/bson.h>
#include <string.h>

#include "bson.h"

PG_MODULE_MAGIC;

/*
 * Function declarations for PostgreSQL
 */
PG_FUNCTION_INFO_V1(bson_in);
PG_FUNCTION_INFO_V1(bson_out);
PG_FUNCTION_INFO_V1(bson_recv);
PG_FUNCTION_INFO_V1(bson_send);
PG_FUNCTION_INFO_V1(bson_cmp);
PG_FUNCTION_INFO_V1(bson_eq);
PG_FUNCTION_INFO_V1(bson_lt);
PG_FUNCTION_INFO_V1(bson_le);
PG_FUNCTION_INFO_V1(bson_gt);
PG_FUNCTION_INFO_V1(bson_ge);
PG_FUNCTION_INFO_V1(bson_ne);
PG_FUNCTION_INFO_V1(bson_hash);
PG_FUNCTION_INFO_V1(bson_get);
PG_FUNCTION_INFO_V1(bson_get_text);
PG_FUNCTION_INFO_V1(bson_exists);
PG_FUNCTION_INFO_V1(bson_exists_any);
PG_FUNCTION_INFO_V1(bson_exists_all);
PG_FUNCTION_INFO_V1(bson_contains);
PG_FUNCTION_INFO_V1(bson_contained);


/*
 * bson_in
 *		Convert a string to internal BSON representation
 *
 * The input string is expected to be in JSON format, which is then
 * converted to BSON using the libbson C driver and stored as bytea.
 */
Datum
bson_in(PG_FUNCTION_ARGS)
{
	char	   *str = PG_GETARG_CSTRING(0);
	bson_t	   *bson_doc;
	bson_error_t error;
	bytea	   *result;
	const uint8_t *data;
	uint32_t	length;

	/* Create BSON document from JSON string */
	bson_doc = bson_new_from_json((const uint8_t *) str, -1, &error);
	if (bson_doc == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type bson: \"%s\"", str),
				 errdetail("JSON parse error: %s", error.message)));

	/* Get raw BSON data */
	data = bson_get_data(bson_doc);
	length = bson_doc->len;

	/* Create PostgreSQL bytea */
	result = (bytea *) palloc(VARHDRSZ + length);
	SET_VARSIZE(result, VARHDRSZ + length);
	memcpy(VARDATA(result), data, length);

	bson_destroy(bson_doc);

	PG_RETURN_BYTEA_P(result);
}


/*
 * bson_out
 *		Convert internal BSON representation to string
 *
 * The internal BSON data (stored as bytea) is converted to its
 * canonical extended JSON representation for output.
 */
Datum
bson_out(PG_FUNCTION_ARGS)
{
	bytea	   *arg = PG_GETARG_BYTEA_PP(0);
	bson_t		bson_doc;
	char	   *json_str;
	char	   *result;

	/* Initialize BSON document from stored data */
	if (!bson_init_static(&bson_doc, (const uint8_t *) VARDATA_ANY(arg),
						  VARSIZE_ANY_EXHDR(arg)))
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("invalid BSON data")));

	/* Convert BSON to canonical extended JSON */
	json_str = bson_as_canonical_extended_json(&bson_doc, NULL);
	if (json_str == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("could not convert BSON to JSON")));

	/* Copy to PostgreSQL memory context */
	result = pstrdup(json_str);
	bson_free(json_str);

	PG_RETURN_CSTRING(result);
}


/*
 * bson_recv
 *		Receive function for binary input
 *
 * Reads BSON data from a binary input stream.
 */
Datum
bson_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	bytea	   *result;
	int			nbytes;

	nbytes = buf->len - buf->cursor;
	result = (bytea *) palloc(VARHDRSZ + nbytes);
	SET_VARSIZE(result, VARHDRSZ + nbytes);
	pq_copymsgbytes(buf, VARDATA(result), nbytes);

	PG_RETURN_BYTEA_P(result);
}


/*
 * bson_send
 *		Send function for binary output
 *
 * Writes BSON data to a binary output stream.
 */
Datum
bson_send(PG_FUNCTION_ARGS)
{
	bytea	   *arg = PG_GETARG_BYTEA_PP(0);
	StringInfoData buf;

	pq_begintypsend(&buf);
	pq_sendbytes(&buf, VARDATA_ANY(arg), VARSIZE_ANY_EXHDR(arg));
	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}


/*
 * bson_cmp
 *		Comparison function for BSON values
 *
 * Compares two BSON documents using binary comparison of the raw data.
 * Returns <0, 0, or >0 for less than, equal, or greater than.
 */
Datum
bson_cmp(PG_FUNCTION_ARGS)
{
	bytea	   *arg1 = PG_GETARG_BYTEA_PP(0);
	bytea	   *arg2 = PG_GETARG_BYTEA_PP(1);
	bson_t		bson1,
				bson2;
	int			result;
	int			len1,
				len2;

	/* Validate BSON data integrity */
	if (!bson_init_static(&bson1, (const uint8_t *) VARDATA_ANY(arg1),
						  VARSIZE_ANY_EXHDR(arg1)))
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("invalid BSON data in first argument")));

	if (!bson_init_static(&bson2, (const uint8_t *) VARDATA_ANY(arg2),
						  VARSIZE_ANY_EXHDR(arg2)))
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("invalid BSON data in second argument")));

	/* Compare BSON documents using memcmp on raw data */
	len1 = VARSIZE_ANY_EXHDR(arg1);
	len2 = VARSIZE_ANY_EXHDR(arg2);
	result = memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), Min(len1, len2));

	if (result == 0)
	{
		/* If common prefix is equal, compare lengths */
		if (len1 < len2)
			result = -1;
		else if (len1 > len2)
			result = 1;
	}

	PG_RETURN_INT32(result);
}


/*
 * bson_eq
 *		Equality function for BSON values
 *
 * Returns true if two BSON documents are identical.
 */
Datum
bson_eq(PG_FUNCTION_ARGS)
{
	bytea	   *arg1 = PG_GETARG_BYTEA_PP(0);
	bytea	   *arg2 = PG_GETARG_BYTEA_PP(1);
	int			len1,
				len2;
	bool		result;

	len1 = VARSIZE_ANY_EXHDR(arg1);
	len2 = VARSIZE_ANY_EXHDR(arg2);

	/* If lengths differ, documents cannot be equal */
	if (len1 != len2)
		result = false;
	else
		result = (memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), len1) == 0);

	PG_RETURN_BOOL(result);
}


/*
 * bson_lt, bson_le, bson_gt, bson_ge, bson_ne
 *		Comparison operators for BSON values
 *
 * These functions implement the standard comparison operators
 * by delegating to bson_cmp or bson_eq as appropriate.
 */
Datum
bson_lt(PG_FUNCTION_ARGS)
{
	Datum		cmp_result = DirectFunctionCall2(bson_cmp,
												 PG_GETARG_DATUM(0),
												 PG_GETARG_DATUM(1));

	PG_RETURN_BOOL(DatumGetInt32(cmp_result) < 0);
}

Datum
bson_le(PG_FUNCTION_ARGS)
{
	Datum		cmp_result = DirectFunctionCall2(bson_cmp,
												 PG_GETARG_DATUM(0),
												 PG_GETARG_DATUM(1));

	PG_RETURN_BOOL(DatumGetInt32(cmp_result) <= 0);
}

Datum
bson_gt(PG_FUNCTION_ARGS)
{
	Datum		cmp_result = DirectFunctionCall2(bson_cmp,
												 PG_GETARG_DATUM(0),
												 PG_GETARG_DATUM(1));

	PG_RETURN_BOOL(DatumGetInt32(cmp_result) > 0);
}

Datum
bson_ge(PG_FUNCTION_ARGS)
{
	Datum		cmp_result = DirectFunctionCall2(bson_cmp,
												 PG_GETARG_DATUM(0),
												 PG_GETARG_DATUM(1));

	PG_RETURN_BOOL(DatumGetInt32(cmp_result) >= 0);
}

Datum
bson_ne(PG_FUNCTION_ARGS)
{
	Datum		eq_result = DirectFunctionCall2(bson_eq,
												PG_GETARG_DATUM(0),
												PG_GETARG_DATUM(1));

	PG_RETURN_BOOL(!DatumGetBool(eq_result));
}


/*
 * bson_hash
 *		Hash function for BSON values
 *
 * Computes a hash value for a BSON document, enabling the use
 * of hash indexes and hash joins.
 */
Datum
bson_hash(PG_FUNCTION_ARGS)
{
	bytea	   *arg = PG_GETARG_BYTEA_PP(0);
	Datum		result;

	result = hash_any((unsigned char *) VARDATA_ANY(arg),
					  VARSIZE_ANY_EXHDR(arg));

	PG_RETURN_DATUM(result);
}


/*
 * bson_get
 *		Extract a field value from a BSON document
 *
 * Given a BSON document and a field name, returns the value of that
 * field as a new BSON document containing just that value.
 */
Datum
bson_get(PG_FUNCTION_ARGS)
{
	bytea	   *bson_data = PG_GETARG_BYTEA_PP(0);
	text	   *path_text = PG_GETARG_TEXT_PP(1);
	bson_t		bson_doc;
	bson_iter_t iter;
	char	   *path;
	bytea	   *result;
	bson_t	   *result_bson;
	const uint8_t *data;
	uint32_t	length;

	/* Extract path string */
	path = text_to_cstring(path_text);

	/* Initialize BSON document from stored data */
	data = (const uint8_t *) VARDATA_ANY(bson_data);
	length = VARSIZE_ANY_EXHDR(bson_data);

	if (!bson_init_static(&bson_doc, data, length))
	{
		pfree(path);
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("invalid BSON data")));
	}

	/* Find the specified field */
	if (!bson_iter_init_find(&iter, &bson_doc, path))
	{
		pfree(path);
		PG_RETURN_NULL();
	}

	/* Create new BSON document with just this field value */
	result_bson = bson_new();

	switch (bson_iter_type(&iter))
	{
		case BSON_TYPE_UTF8:
			{
				uint32_t	str_len;
				const char *str_val = bson_iter_utf8(&iter, &str_len);

				bson_append_utf8(result_bson, "value", 5, str_val, str_len);
				break;
			}
		case BSON_TYPE_INT32:
			{
				int32_t		int_val = bson_iter_int32(&iter);

				bson_append_int32(result_bson, "value", 5, int_val);
				break;
			}
		case BSON_TYPE_INT64:
			{
				int64_t		int64_val = bson_iter_int64(&iter);

				bson_append_int64(result_bson, "value", 5, int64_val);
				break;
			}
		case BSON_TYPE_DOUBLE:
			{
				double		double_val = bson_iter_double(&iter);

				bson_append_double(result_bson, "value", 5, double_val);
				break;
			}
		case BSON_TYPE_BOOL:
			{
				bool		bool_val = bson_iter_bool(&iter);

				bson_append_bool(result_bson, "value", 5, bool_val);
				break;
			}
		case BSON_TYPE_NULL:
			{
				bson_append_null(result_bson, "value", 5);
				break;
			}
		case BSON_TYPE_DOCUMENT:
			{
				bson_t		subdoc;
				uint32_t	doc_len;
				const uint8_t *doc_data;

				bson_iter_document(&iter, &doc_len, &doc_data);
				bson_init_static(&subdoc, doc_data, doc_len);
				bson_append_document(result_bson, "value", 5, &subdoc);
				break;
			}
		case BSON_TYPE_ARRAY:
			{
				bson_t		subarray;
				uint32_t	array_len;
				const uint8_t *array_data;

				bson_iter_array(&iter, &array_len, &array_data);
				bson_init_static(&subarray, array_data, array_len);
				bson_append_array(result_bson, "value", 5, &subarray);
				break;
			}
		default:
			{
				bson_append_null(result_bson, "value", 5);
				break;
			}
	}

	/* Convert result BSON to bytea */
	data = bson_get_data(result_bson);
	length = result_bson->len;

	result = (bytea *) palloc(VARHDRSZ + length);
	SET_VARSIZE(result, VARHDRSZ + length);
	memcpy(VARDATA(result), data, length);

	bson_destroy(result_bson);
	pfree(path);

	PG_RETURN_BYTEA_P(result);
}


/*
 * bson_get_text
 *		Extract a field value from BSON document as text
 *
 * Similar to bson_get, but returns the field value converted to
 * PostgreSQL text type for easier consumption in SQL queries.
 */
Datum
bson_get_text(PG_FUNCTION_ARGS)
{
    bytea *bson_data = PG_GETARG_BYTEA_PP(0);
    text *path_text = PG_GETARG_TEXT_PP(1);
    bson_t bson;
    bson_iter_t iter;
    char *path;
    text *result;
    char *str_result;
    const uint8_t *data;
    uint32_t length;
    
    /* Extract path string */
    path = text_to_cstring(path_text);
    
    /* Initialize BSON from stored data */
    data = (const uint8_t *)VARDATA_ANY(bson_data);
    length = VARSIZE_ANY_EXHDR(bson_data);
    
    if (!bson_init_static(&bson, data, length))
    {
        ereport(ERROR,
                (errcode(ERRCODE_DATA_CORRUPTED),
                 errmsg("invalid BSON data")));
    }
    
    /* Find the field */
    if (!bson_iter_init_find(&iter, &bson, path))
    {
        pfree(path);
        PG_RETURN_NULL();
    }
    
    /* Convert to text based on type */
    switch (bson_iter_type(&iter))
    {
        case BSON_TYPE_UTF8:
        {
            uint32_t str_len;
            const char *str = bson_iter_utf8(&iter, &str_len);
            result = cstring_to_text_with_len(str, str_len);
            break;
        }
        case BSON_TYPE_INT32:
        {
            int32_t val = bson_iter_int32(&iter);
            str_result = psprintf("%d", val);
            result = cstring_to_text(str_result);
            pfree(str_result);
            break;
        }
        case BSON_TYPE_INT64:
        {
            int64_t val = bson_iter_int64(&iter);
            str_result = psprintf("%lld", (long long)val);
            result = cstring_to_text(str_result);
            pfree(str_result);
            break;
        }
        case BSON_TYPE_DOUBLE:
        {
            double val = bson_iter_double(&iter);
            str_result = psprintf("%g", val);
            result = cstring_to_text(str_result);
            pfree(str_result);
            break;
        }
        case BSON_TYPE_BOOL:
        {
            bool val = bson_iter_bool(&iter);
            result = cstring_to_text(val ? "true" : "false");
            break;
        }
        case BSON_TYPE_NULL:
        {
            result = cstring_to_text("null");
            break;
        }
        default:
        {
            pfree(path);
            PG_RETURN_NULL();
        }
    }
    
    pfree(path);
    PG_RETURN_TEXT_P(result);
}

/* Check if a field exists in BSON document */
Datum
bson_exists(PG_FUNCTION_ARGS)
{
    bytea *bson_data = PG_GETARG_BYTEA_PP(0);
    text *path_text = PG_GETARG_TEXT_PP(1);
    bson_t bson;
    bson_iter_t iter;
    char *path;
    bool result;
    
    /* Extract path string */
    path = text_to_cstring(path_text);
    
    /* Initialize BSON from data */
    if (!bson_init_static(&bson, (const uint8_t *)VARDATA_ANY(bson_data), VARSIZE_ANY_EXHDR(bson_data)))
    {
        ereport(ERROR,
                (errcode(ERRCODE_DATA_CORRUPTED),
                 errmsg("invalid BSON data")));
    }
    
    /* Check if field exists */
    result = bson_iter_init_find(&iter, &bson, path);
    
    pfree(path);
    PG_RETURN_BOOL(result);
}

/* Check if any of the fields exist in BSON document */
Datum
bson_exists_any(PG_FUNCTION_ARGS)
{
    bytea *bson_data = PG_GETARG_BYTEA_PP(0);
    ArrayType *paths_array = PG_GETARG_ARRAYTYPE_P(1);
    bson_t bson;
    bson_iter_t iter;
    bool result = false;
    Datum *elements;
    bool *nulls;
    int num_elements;
    int16 typlen;
    bool typbyval;
    char typalign;
    
    /* Initialize BSON from data */
    if (!bson_init_static(&bson, (const uint8_t *)VARDATA_ANY(bson_data), VARSIZE_ANY_EXHDR(bson_data)))
    {
        ereport(ERROR,
                (errcode(ERRCODE_DATA_CORRUPTED),
                 errmsg("invalid BSON data")));
    }
    
    /* Deconstruct array */
    get_typlenbyvalalign(TEXTOID, &typlen, &typbyval, &typalign);
    deconstruct_array(paths_array, TEXTOID, typlen, typbyval, typalign,
                      &elements, &nulls, &num_elements);
    
    /* Check if any path exists */
    for (int i = 0; i < num_elements && !result; i++)
    {
        if (!nulls[i])
        {
            char *path = TextDatumGetCString(elements[i]);
            result = bson_iter_init_find(&iter, &bson, path);
            pfree(path);
        }
    }
    
    pfree(elements);
    pfree(nulls);
    PG_RETURN_BOOL(result);
}

/* Check if all of the fields exist in BSON document */
Datum
bson_exists_all(PG_FUNCTION_ARGS)
{
    bytea *bson_data = PG_GETARG_BYTEA_PP(0);
    ArrayType *paths_array = PG_GETARG_ARRAYTYPE_P(1);
    bson_t bson;
    bson_iter_t iter;
    bool result = true;
    Datum *elements;
    bool *nulls;
    int num_elements;
    int16 typlen;
    bool typbyval;
    char typalign;
    
    /* Initialize BSON from data */
    if (!bson_init_static(&bson, (const uint8_t *)VARDATA_ANY(bson_data), VARSIZE_ANY_EXHDR(bson_data)))
    {
        ereport(ERROR,
                (errcode(ERRCODE_DATA_CORRUPTED),
                 errmsg("invalid BSON data")));
    }
    
    /* Deconstruct array */
    get_typlenbyvalalign(TEXTOID, &typlen, &typbyval, &typalign);
    deconstruct_array(paths_array, TEXTOID, typlen, typbyval, typalign,
                      &elements, &nulls, &num_elements);
    
    /* Check if all paths exist */
    for (int i = 0; i < num_elements && result; i++)
    {
        if (!nulls[i])
        {
            char *path = TextDatumGetCString(elements[i]);
            result = bson_iter_init_find(&iter, &bson, path);
            pfree(path);
        }
        else
        {
            result = false;
        }
    }
    
    pfree(elements);
    pfree(nulls);
    PG_RETURN_BOOL(result);
}

/* Check if first BSON contains the second BSON */
Datum
bson_contains(PG_FUNCTION_ARGS)
{
    bytea *bson1_data = PG_GETARG_BYTEA_PP(0);
    bytea *bson2_data = PG_GETARG_BYTEA_PP(1);
    bson_t bson1, bson2;
    bson_iter_t iter1, iter2;
    bool result = true;
    
    /* Initialize BSON objects from data */
    if (!bson_init_static(&bson1, (const uint8_t *)VARDATA_ANY(bson1_data), VARSIZE_ANY_EXHDR(bson1_data)))
    {
        ereport(ERROR,
                (errcode(ERRCODE_DATA_CORRUPTED),
                 errmsg("invalid BSON data in first argument")));
    }
    
    if (!bson_init_static(&bson2, (const uint8_t *)VARDATA_ANY(bson2_data), VARSIZE_ANY_EXHDR(bson2_data)))
    {
        ereport(ERROR,
                (errcode(ERRCODE_DATA_CORRUPTED),
                 errmsg("invalid BSON data in second argument")));
    }
    
    /* Check if bson1 contains all key-value pairs from bson2 */
    if (bson_iter_init(&iter2, &bson2))
    {
        while (bson_iter_next(&iter2) && result)
        {
            const char *key = bson_iter_key(&iter2);
            
            if (bson_iter_init_find(&iter1, &bson1, key))
            {
                /* Simple value comparison - compare types and values */
                if (bson_iter_type(&iter1) != bson_iter_type(&iter2))
                    result = false;
                else
                {
                    /* Compare values based on type */
                    switch (bson_iter_type(&iter1))
                    {
                        case BSON_TYPE_UTF8:
                        {
                            uint32_t len1, len2;
                            const char *str1 = bson_iter_utf8(&iter1, &len1);
                            const char *str2 = bson_iter_utf8(&iter2, &len2);
                            if (len1 != len2 || memcmp(str1, str2, len1) != 0)
                                result = false;
                            break;
                        }
                        case BSON_TYPE_INT32:
                            if (bson_iter_int32(&iter1) != bson_iter_int32(&iter2))
                                result = false;
                            break;
                        case BSON_TYPE_INT64:
                            if (bson_iter_int64(&iter1) != bson_iter_int64(&iter2))
                                result = false;
                            break;
                        case BSON_TYPE_DOUBLE:
                            if (bson_iter_double(&iter1) != bson_iter_double(&iter2))
                                result = false;
                            break;
                        case BSON_TYPE_BOOL:
                            if (bson_iter_bool(&iter1) != bson_iter_bool(&iter2))
                                result = false;
                            break;
                        default:
                            /* For other types, just assume they're different */
                            result = false;
                            break;
                    }
                }
            }
            else
            {
                result = false;
            }
        }
    }
    
    PG_RETURN_BOOL(result);
}

/* Check if first BSON is contained in the second BSON */
Datum
bson_contained(PG_FUNCTION_ARGS)
{
    /* bson_contained(A, B) is equivalent to bson_contains(B, A) */
    return DirectFunctionCall2(bson_contains, PG_GETARG_DATUM(1), PG_GETARG_DATUM(0));
}
