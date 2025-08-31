/*
 * bson_op.c
 *
 * PostgreSQL BSON extension - BSON query operators using mongo-c-driver
 *
 * This module implements BSON query operators for working with the BSON type.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    contrib/bson/bson_op.c
 */

#include "postgres.h"

#include <ctype.h>
#include <string.h>

#include "fmgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"

#include <bson/bson.h>

/* BsonData structure (matching bson.c) */
typedef struct BsonData
{
	int32		vl_len_;		/* varlena header (do not touch directly!) */
	int32		size;			/* size of BSON data */
	char		data[FLEXIBLE_ARRAY_MEMBER];  /* BSON binary data */
} BsonData;

/*
 * Helper function to convert BsonData to bson_t
 */
static bson_t *
bson_data_to_bson_t(const BsonData *bson_data)
{
	return bson_new_from_data((const uint8_t *) bson_data->data, bson_data->size);
}

/*
 * BSON query operators implementation using mongo-c-driver
 */

/*
 * Equality operator
 */
PG_FUNCTION_INFO_V1(bson_eq_op);

Datum
bson_eq_op(PG_FUNCTION_ARGS)
{
	BsonData   *left = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *right = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *bson_left, *bson_right;
	char	   *json_left, *json_right;
	bool		result;

	bson_left = bson_data_to_bson_t(left);
	bson_right = bson_data_to_bson_t(right);
	
	if (!bson_left || !bson_right)
	{
		if (bson_left) bson_destroy(bson_left);
		if (bson_right) bson_destroy(bson_right);
		PG_RETURN_BOOL(false);
	}

	/* Compare as canonical JSON strings */
	json_left = bson_as_canonical_extended_json(bson_left, NULL);
	json_right = bson_as_canonical_extended_json(bson_right, NULL);

	result = (strcmp(json_left, json_right) == 0);

	bson_free(json_left);
	bson_free(json_right);
	bson_destroy(bson_left);
	bson_destroy(bson_right);

	PG_RETURN_BOOL(result);
}

/*
 * IN operator - check if value is in array
 */
PG_FUNCTION_INFO_V1(bson_in_op);

Datum
bson_in_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	ArrayType  *array = PG_GETARG_ARRAYTYPE_P(1);
	
	/* For now, implement as simple placeholder */
	/* This would require complex array membership testing */
	PG_RETURN_BOOL(false);
}

#include "fmgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"

#include <bson/bson.h>

/* BsonData structure (matching bson.c) */
typedef struct BsonData
{
	int32		vl_len_;		/* varlena header (do not touch directly!) */
	int32		size;			/* size of BSON data */
	char		data[FLEXIBLE_ARRAY_MEMBER];  /* BSON binary data */
} BsonData;

/*
 * Helper function to convert BsonData to bson_t
 */
static bson_t *
bson_data_to_bson_t(const BsonData *bson_data)
{
	return bson_new_from_data((const uint8_t *) bson_data->data, bson_data->size);
}

/*
 * BSON query operators implementation using mongo-c-driver
 */

/*
 * Equality operator
 */
PG_FUNCTION_INFO_V1(bson_eq_op);

Datum
bson_eq_op(PG_FUNCTION_ARGS)
{
	BsonData   *left = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *right = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *bson_left, *bson_right;
	char	   *json_left, *json_right;
	bool		result;

	bson_left = bson_data_to_bson_t(left);
	bson_right = bson_data_to_bson_t(right);
	
	if (!bson_left || !bson_right)
	{
		if (bson_left) bson_destroy(bson_left);
		if (bson_right) bson_destroy(bson_right);
		PG_RETURN_BOOL(false);
	}

	/* Compare as canonical JSON strings */
	json_left = bson_as_canonical_extended_json(bson_left, NULL);
	json_right = bson_as_canonical_extended_json(bson_right, NULL);

	result = (strcmp(json_left, json_right) == 0);

	bson_free(json_left);
	bson_free(json_right);
	bson_destroy(bson_left);
	bson_destroy(bson_right);

	PG_RETURN_BOOL(result);
}

/*
 * Inequality operator
 */
PG_FUNCTION_INFO_V1(bson_ne_op);

Datum
bson_ne_op(PG_FUNCTION_ARGS)
{
	BsonData   *left = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *right = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *bson_left, *bson_right;
	char	   *json_left, *json_right;
	bool		result;

	bson_left = bson_data_to_bson_t(left);
	bson_right = bson_data_to_bson_t(right);
	
	if (!bson_left || !bson_right)
	{
		if (bson_left) bson_destroy(bson_left);
		if (bson_right) bson_destroy(bson_right);
		PG_RETURN_BOOL(true);
	}

	/* Compare as canonical JSON strings */
	json_left = bson_as_canonical_extended_json(bson_left, NULL);
	json_right = bson_as_canonical_extended_json(bson_right, NULL);

	result = (strcmp(json_left, json_right) != 0);

	bson_free(json_left);
	bson_free(json_right);
	bson_destroy(bson_left);
	bson_destroy(bson_right);

	PG_RETURN_BOOL(result);
}

/*
 * Greater than operator
 */
PG_FUNCTION_INFO_V1(bson_gt_op);

Datum
bson_gt_op(PG_FUNCTION_ARGS)
{
	BsonData   *left = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *right = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *bson_left, *bson_right;
	char	   *json_left, *json_right;
	bool		result;

	bson_left = bson_data_to_bson_t(left);
	bson_right = bson_data_to_bson_t(right);
	
	if (!bson_left || !bson_right)
	{
		if (bson_left) bson_destroy(bson_left);
		if (bson_right) bson_destroy(bson_right);
		PG_RETURN_BOOL(false);
	}

	/* Compare as canonical JSON strings */
	json_left = bson_as_canonical_extended_json(bson_left, NULL);
	json_right = bson_as_canonical_extended_json(bson_right, NULL);

	result = (strcmp(json_left, json_right) > 0);

	bson_free(json_left);
	bson_free(json_right);
	bson_destroy(bson_left);
	bson_destroy(bson_right);

	PG_RETURN_BOOL(result);
}

/*
 * Less than operator
 */
PG_FUNCTION_INFO_V1(bson_lt_op);

Datum
bson_lt_op(PG_FUNCTION_ARGS)
{
	BsonData   *left = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *right = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *bson_left, *bson_right;
	char	   *json_left, *json_right;
	bool		result;

	bson_left = bson_data_to_bson_t(left);
	bson_right = bson_data_to_bson_t(right);
	
	if (!bson_left || !bson_right)
	{
		if (bson_left) bson_destroy(bson_left);
		if (bson_right) bson_destroy(bson_right);
		PG_RETURN_BOOL(false);
	}

	/* Compare as canonical JSON strings */
	json_left = bson_as_canonical_extended_json(bson_left, NULL);
	json_right = bson_as_canonical_extended_json(bson_right, NULL);

	result = (strcmp(json_left, json_right) < 0);

	bson_free(json_left);
	bson_free(json_right);
	bson_destroy(bson_left);
	bson_destroy(bson_right);

	PG_RETURN_BOOL(result);
}

/*
 * Greater than or equal operator
 */
PG_FUNCTION_INFO_V1(bson_gte_op);

Datum
bson_gte_op(PG_FUNCTION_ARGS)
{
	BsonData   *left = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *right = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *bson_left, *bson_right;
	char	   *json_left, *json_right;
	bool		result;

	bson_left = bson_data_to_bson_t(left);
	bson_right = bson_data_to_bson_t(right);
	
	if (!bson_left || !bson_right)
	{
		if (bson_left) bson_destroy(bson_left);
		if (bson_right) bson_destroy(bson_right);
		PG_RETURN_BOOL(false);
	}

	/* Compare as canonical JSON strings */
	json_left = bson_as_canonical_extended_json(bson_left, NULL);
	json_right = bson_as_canonical_extended_json(bson_right, NULL);

	result = (strcmp(json_left, json_right) >= 0);

	bson_free(json_left);
	bson_free(json_right);
	bson_destroy(bson_left);
	bson_destroy(bson_right);

	PG_RETURN_BOOL(result);
}

/*
 * Less than or equal operator
 */
PG_FUNCTION_INFO_V1(bson_lte_op);

Datum
bson_lte_op(PG_FUNCTION_ARGS)
{
	BsonData   *left = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *right = (BsonData *) PG_GETARG_POINTER(1);
	bson_t	   *bson_left, *bson_right;
	char	   *json_left, *json_right;
	bool		result;

	bson_left = bson_data_to_bson_t(left);
	bson_right = bson_data_to_bson_t(right);
	
	if (!bson_left || !bson_right)
	{
		if (bson_left) bson_destroy(bson_left);
		if (bson_right) bson_destroy(bson_right);
		PG_RETURN_BOOL(false);
	}

	/* Compare as canonical JSON strings */
	json_left = bson_as_canonical_extended_json(bson_left, NULL);
	json_right = bson_as_canonical_extended_json(bson_right, NULL);

	result = (strcmp(json_left, json_right) <= 0);

	bson_free(json_left);
	bson_free(json_right);
	bson_destroy(bson_left);
	bson_destroy(bson_right);

	PG_RETURN_BOOL(result);
}

/*
 * IN operator - check if value is in array
 */
PG_FUNCTION_INFO_V1(bson_in_op);

Datum
bson_in_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	ArrayType  *array = PG_GETARG_ARRAYTYPE_P(1);
	
	/* For now, implement as simple equality check with first array element */
	/* This is a placeholder implementation */
	PG_RETURN_BOOL(false);
}

/*
 * Type operator - check value type
 */
PG_FUNCTION_INFO_V1(bson_type_op);

Datum
bson_type_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	int32		type_code = PG_GETARG_INT32(1);
	bson_t	   *bson;
	bson_iter_t iter;
	bson_type_t actual_type;
	bool		result = false;

	bson = bson_data_to_bson_t(bson_data);
	if (!bson)
		PG_RETURN_BOOL(false);

	/* For wrapped standalone values, check the "value" field */
	if (bson_iter_init_find(&iter, bson, "value"))
	{
		actual_type = bson_iter_type(&iter);
		result = (actual_type == type_code);
	}
	else if (bson_iter_init(&iter, bson))
	{
		/* For documents, check the first field's type */
		if (bson_iter_next(&iter))
		{
			actual_type = bson_iter_type(&iter);
			result = (actual_type == type_code);
		}
	}

	bson_destroy(bson);
	PG_RETURN_BOOL(result);
}

/*
 * Exists operator - check if field exists
 */
PG_FUNCTION_INFO_V1(bson_exists_op);

Datum
bson_exists_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	text	   *field_name = PG_GETARG_TEXT_P(1);
	char	   *field_str;
	bson_t	   *bson;
	bson_iter_t iter;
	bool		result;

	field_str = text_to_cstring(field_name);
	bson = bson_data_to_bson_t(bson_data);
	if (!bson)
	{
		pfree(field_str);
		PG_RETURN_BOOL(false);
	}

	result = bson_iter_init_find(&iter, bson, field_str);
	
	bson_destroy(bson);
	pfree(field_str);
	PG_RETURN_BOOL(result);
}

/*
 * Regex operator - check regex pattern
 */
PG_FUNCTION_INFO_V1(bson_regex_op);

Datum
bson_regex_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	text	   *pattern = PG_GETARG_TEXT_P(1);
	char	   *pattern_str;
	bson_t	   *bson;
	bson_iter_t iter;
	const char *str_value;
	bool		result = false;

	pattern_str = text_to_cstring(pattern);
	bson = bson_data_to_bson_t(bson_data);
	if (!bson)
	{
		pfree(pattern_str);
		PG_RETURN_BOOL(false);
	}

	/* Check if it's a wrapped standalone string value */
	if (bson_iter_init_find(&iter, bson, "value") && 
		BSON_ITER_HOLDS_UTF8(&iter))
	{
		str_value = bson_iter_utf8(&iter, NULL);
		/* Simple substring matching */
		result = strstr(str_value, pattern_str) != NULL;
	}
	else if (bson_iter_init(&iter, bson))
	{
		/* For documents, check the first string field */
		while (bson_iter_next(&iter))
		{
			if (BSON_ITER_HOLDS_UTF8(&iter))
			{
				str_value = bson_iter_utf8(&iter, NULL);
				result = strstr(str_value, pattern_str) != NULL;
				break;
			}
		}
	}

	bson_destroy(bson);
	pfree(pattern_str);
	PG_RETURN_BOOL(result);
}

/*
 * Modulo operator - check modulo
 */
PG_FUNCTION_INFO_V1(bson_mod_op);

Datum
bson_mod_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	int32		divisor = PG_GETARG_INT32(1);
	int32		remainder = PG_GETARG_INT32(2);
	bson_t	   *bson;
	bson_iter_t iter;
	bool		result = false;

	if (divisor == 0)
		PG_RETURN_BOOL(false);

	bson = bson_data_to_bson_t(bson_data);
	if (!bson)
		PG_RETURN_BOOL(false);

	/* Check if it's a wrapped standalone numeric value */
	if (bson_iter_init_find(&iter, bson, "value"))
	{
		if (BSON_ITER_HOLDS_INT32(&iter))
		{
			int32 value = bson_iter_int32(&iter);
			result = (value % divisor) == remainder;
		}
		else if (BSON_ITER_HOLDS_INT64(&iter))
		{
			int64 value = bson_iter_int64(&iter);
			result = (value % divisor) == remainder;
		}
		else if (BSON_ITER_HOLDS_DOUBLE(&iter))
		{
			double value = bson_iter_double(&iter);
			result = (((int64)value) % divisor) == remainder;
		}
	}

	bson_destroy(bson);
	PG_RETURN_BOOL(result);
}

/*
 * Size operator - check array size
 */
PG_FUNCTION_INFO_V1(bson_size_op);

Datum
bson_size_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	int32		expected_size = PG_GETARG_INT32(1);
	bson_t	   *bson;
	bson_iter_t iter;
	bson_iter_t array_iter;
	int32		actual_size = 0;
	bool		result = false;

	bson = bson_data_to_bson_t(bson_data);
	if (!bson)
		PG_RETURN_BOOL(false);

	/* Look for array field or wrapped array value */
	if (bson_iter_init_find(&iter, bson, "value") && 
		BSON_ITER_HOLDS_ARRAY(&iter))
	{
		bson_iter_recurse(&iter, &array_iter);
		while (bson_iter_next(&array_iter))
			actual_size++;
		result = (actual_size == expected_size);
	}
	else if (bson_iter_init(&iter, bson))
	{
		/* Check first array field in document */
		while (bson_iter_next(&iter))
		{
			if (BSON_ITER_HOLDS_ARRAY(&iter))
			{
				bson_iter_recurse(&iter, &array_iter);
				while (bson_iter_next(&array_iter))
					actual_size++;
				result = (actual_size == expected_size);
				break;
			}
		}
	}

	bson_destroy(bson);
	PG_RETURN_BOOL(result);
}

/*
 * All operator - check if array contains all elements
 */
PG_FUNCTION_INFO_V1(bson_all_op);

Datum
bson_all_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	ArrayType  *array = PG_GETARG_ARRAYTYPE_P(1);
	
	/* For now, implement as simple placeholder */
	/* This would require complex array matching logic */
	PG_RETURN_BOOL(false);
}

/*
 * Element match operator - check if array element matches query
 */
PG_FUNCTION_INFO_V1(bson_elem_match_op);

Datum
bson_elem_match_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	BsonData   *query_data = (BsonData *) PG_GETARG_POINTER(1);
	
	/* For now, implement as simple placeholder */
	/* This would require complex array element matching logic */
	PG_RETURN_BOOL(false);
}

/*
 * Not IN operator - check if value is not in array
 */
PG_FUNCTION_INFO_V1(bson_nin_op);

Datum
bson_nin_op(PG_FUNCTION_ARGS)
{
	BsonData   *bson_data = (BsonData *) PG_GETARG_POINTER(0);
	ArrayType  *array = PG_GETARG_ARRAYTYPE_P(1);
	
	/* For now, implement as simple placeholder */
	/* This is the inverse of bson_in_op */
	PG_RETURN_BOOL(true);
}
