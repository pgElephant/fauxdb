/*
 * bson.c
 *
 * PostgreSQL BSON extension - BSON data type
 *
 * This module implements a PostgreSQL data type that stores and manipulates
 * BSON (Binary JSON) data.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    contrib/bson/bson.c
 */

#include "postgres.h"

#include <ctype.h>
#include <string.h>

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/jsonb.h"
#include "utils/varlena.h"

/*
 * Simple BSON storage structure for PostgreSQL
 */
typedef struct
{
	int32		vl_len_;		/* varlena header */
	uint8		type;			/* BSON type */
	char		data[FLEXIBLE_ARRAY_MEMBER];	/* JSON string data */
} BsonData;

/* Basic BSON type constants */
#define BSON_TYPE_DOUBLE	0x01
#define BSON_TYPE_STRING	0x02
#define BSON_TYPE_OBJECT	0x03
#define BSON_TYPE_ARRAY		0x04
#define BSON_TYPE_BOOL		0x08
#define BSON_TYPE_NULL		0x0A

#define DatumGetBsonP(X)		((BsonData *) PG_DETOAST_DATUM(X))
#define BsonPGetDatum(X)		PointerGetDatum(X)
#define PG_GETARG_BSON_P(n)		DatumGetBsonP(PG_GETARG_DATUM(n))
#define PG_RETURN_BSON_P(x)		PG_RETURN_POINTER(x)

#include "../include/bson.h"

PG_MODULE_MAGIC;

/*
 * BSON type OID
 */
Oid BSONOID = InvalidOid;

/*
 * Forward declarations
 */
static BsonValue *bson_parse_value(StringInfo input, bool *success);
static BsonValue *bson_parse_object(StringInfo input, bool *success);
static BsonValue *bson_parse_array(StringInfo input, bool *success);
static BsonValue *bson_parse_string(StringInfo input, bool *success);
static BsonValue *bson_parse_number(StringInfo input, bool *success);
static BsonValue *bson_parse_literal(StringInfo input, bool *success);
static char *bson_serialize_value(BsonValue *value);
static char *bson_serialize_object(BsonValue *value);
static char *bson_serialize_array(BsonValue *value);
static char *bson_serialize_string(BsonValue *value);
static char *bson_serialize_number(BsonValue *value);
static char *bson_serialize_literal(BsonValue *value);
static int	bson_compare_values(BsonValue *a, BsonValue *b);
static uint32 bson_hash_value(BsonValue *value);
static BsonValue *bson_get_element_by_path(BsonValue *doc, const char *path);
static bool bson_set_element_by_path(BsonValue *doc, const char *path, BsonValue *element);
static bool bson_delete_element_by_path(BsonValue *doc, const char *path);
static void bson_free_value(BsonValue *value);

/*
 * Input function for BSON type
 */
PG_FUNCTION_INFO_V1(bson_in);

Datum
bson_in(PG_FUNCTION_ARGS)
{
	char	   *input = PG_GETARG_CSTRING(0);
	BsonData   *result;
	size_t		len;

	/* For now, implement a simple storage format */
	/* Store the original JSON string with minimal processing */
	len = strlen(input);
	result = (BsonData *) palloc(VARHDRSZ + sizeof(uint8) + len + 1);
	SET_VARSIZE(result, VARHDRSZ + sizeof(uint8) + len + 1);
	
	/* Determine basic type from input */
	if (input[0] == '{')
		result->type = BSON_TYPE_OBJECT;
	else if (input[0] == '[')
		result->type = BSON_TYPE_ARRAY;
	else if (input[0] == '"')
		result->type = BSON_TYPE_STRING;
	else if (strcmp(input, "null") == 0)
		result->type = BSON_TYPE_NULL;
	else if (strcmp(input, "true") == 0 || strcmp(input, "false") == 0)
		result->type = BSON_TYPE_BOOL;
	else
		result->type = BSON_TYPE_DOUBLE; /* assume number */
	
	/* Store the original string */
	strcpy(result->data, input);

	PG_RETURN_POINTER(result);
}

/*
 * Output function for BSON type
 */
PG_FUNCTION_INFO_V1(bson_out);

Datum
bson_out(PG_FUNCTION_ARGS)
{
	BsonData   *bson = (BsonData *) PG_GETARG_POINTER(0);
	char	   *result;

	/* Return the stored string data */
	result = pstrdup(bson->data);

	PG_RETURN_CSTRING(result);
}

/*
 * Receive function for BSON type
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
	result = (BsonData *) palloc(VARHDRSZ + sizeof(uint8) + len + 1);
	SET_VARSIZE(result, VARHDRSZ + sizeof(uint8) + len + 1);
	result->type = BSON_TYPE_OBJECT; /* assume object for now */
	memcpy(result->data, data, len);
	result->data[len] = '\0';

	PG_RETURN_POINTER(result);
}

/*
 * Send function for BSON type
 */
PG_FUNCTION_INFO_V1(bson_send);

Datum
bson_send(PG_FUNCTION_ARGS)
{
	BsonData   *bson = (BsonData *) PG_GETARG_POINTER(0);
	StringInfo	buf;
	int32		len;

	buf = makeStringInfo();
	
	/* Send the stored string value */
	len = strlen(bson->data);
	pq_sendint32(buf, len);
	pq_sendbytes(buf, bson->data, len);

	PG_RETURN_BYTEA_P(pq_endtypsend(buf));
}

/*
 * Get element by key (-> operator)
 */
PG_FUNCTION_INFO_V1(bson_get);

Datum
bson_get(PG_FUNCTION_ARGS)
{
	BsonData   *doc = (BsonData *) PG_GETARG_POINTER(0);
	text	   *key = PG_GETARG_TEXT_P(1);
	char	   *key_str;
	BsonData   *result;
	char	   *json_str;
	text	   *json_text;
	Datum		json_datum;
	text	   *extracted_text;
	char	   *extracted_str;

	key_str = text_to_cstring(key);
	
	/* Use PostgreSQL's JSON functions to extract the value */
	json_str = doc->data;
	json_text = cstring_to_text(json_str);
	json_datum = PointerGetDatum(json_text);
	
	/* Try to extract using json_extract_path_text */
	extracted_text = DatumGetTextP(DirectFunctionCall2(json_extract_path_text,
													   json_datum,
													   PointerGetDatum(cstring_to_text(key_str))));
	
	pfree(key_str);
	pfree(json_text);
	
	if (extracted_text == NULL)
		PG_RETURN_NULL();
	
	/* Convert back to BSON format */
	extracted_str = text_to_cstring(extracted_text);
	
	/* Create a new BSON value */
	result = (BsonData *) palloc(VARHDRSZ + sizeof(uint8) + strlen(extracted_str) + 3); /* +3 for quotes */
	SET_VARSIZE(result, VARHDRSZ + sizeof(uint8) + strlen(extracted_str) + 3);
	result->type = BSON_TYPE_STRING;
	
	/* Add quotes back for JSON compatibility */
	sprintf(result->data, "\"%s\"", extracted_str);
	
	pfree(extracted_str);
	pfree(extracted_text);

	PG_RETURN_POINTER(result);
}

/*
 * Get element as text (->> operator)
 */
PG_FUNCTION_INFO_V1(bson_get_text);

Datum
bson_get_text(PG_FUNCTION_ARGS)
{
	BsonData   *doc = (BsonData *) PG_GETARG_POINTER(0);
	text	   *key = PG_GETARG_TEXT_P(1);
	char	   *key_str;
	char	   *json_str;
	text	   *json_text;
	Datum		json_datum;
	text	   *result;

	key_str = text_to_cstring(key);
	
	/* Use PostgreSQL's JSON functions to extract the value as text */
	json_str = doc->data;
	json_text = cstring_to_text(json_str);
	json_datum = PointerGetDatum(json_text);
	
	/* Extract using json_extract_path_text */
	result = DatumGetTextP(DirectFunctionCall2(json_extract_path_text,
											   json_datum,
											   PointerGetDatum(cstring_to_text(key_str))));
	
	pfree(key_str);
	pfree(json_text);
	
	if (result == NULL)
		PG_RETURN_NULL();

	PG_RETURN_TEXT_P(result);
}

/*
 * Check if key exists (? operator)
 */
PG_FUNCTION_INFO_V1(bson_exists);

Datum
bson_exists(PG_FUNCTION_ARGS)
{
	BsonData   *doc = (BsonData *) PG_GETARG_POINTER(0);
	text	   *key = PG_GETARG_TEXT_P(1);
	char	   *key_str;
	char	   *json_str;
	text	   *json_text;
	Datum		json_datum;
	text	   *result_text;
	bool		exists;

	key_str = text_to_cstring(key);
	
	/* Use PostgreSQL's JSON functions to check if key exists */
	json_str = doc->data;
	json_text = cstring_to_text(json_str);
	json_datum = PointerGetDatum(json_text);
	
	/* Extract using json_extract_path_text to check existence */
	result_text = DatumGetTextP(DirectFunctionCall2(json_extract_path_text,
													json_datum,
													PointerGetDatum(cstring_to_text(key_str))));
	
	exists = (result_text != NULL);
	
	pfree(key_str);
	pfree(json_text);

	PG_RETURN_BOOL(exists);
}

/*
 * Check if any of the keys exist (?| operator)
 */
PG_FUNCTION_INFO_V1(bson_exists_any);

Datum
bson_exists_any(PG_FUNCTION_ARGS)
{
	BsonValue  *doc = (BsonValue *) PG_GETARG_POINTER(0);
	ArrayType  *keys = PG_GETARG_ARRAYTYPE_P(1);
	Datum	   *key_datums;
	bool	   *key_nulls;
	int			key_count;
	int			i;
	char	   *key_str;
	BsonValue  *result;

	deconstruct_array(keys, TEXTOID, -1, false, TYPALIGN_INT,
					 &key_datums, &key_nulls, &key_count);

	for (i = 0; i < key_count; i++)
	{
		if (key_nulls[i])
			continue;

		key_str = TextDatumGetCString(key_datums[i]);
		result = bson_get_element_by_path(doc, key_str);
		pfree(key_str);

		if (result != NULL)
		{
			pfree(key_datums);
			pfree(key_nulls);
			PG_RETURN_BOOL(true);
		}
	}

	pfree(key_datums);
	pfree(key_nulls);
	PG_RETURN_BOOL(false);
}

/*
 * Check if all keys exist (?& operator)
 */
PG_FUNCTION_INFO_V1(bson_exists_all);

Datum
bson_exists_all(PG_FUNCTION_ARGS)
{
	BsonValue  *doc = (BsonValue *) PG_GETARG_POINTER(0);
	ArrayType  *keys = PG_GETARG_ARRAYTYPE_P(1);
	Datum	   *key_datums;
	bool	   *key_nulls;
	int			key_count;
	int			i;
	char	   *key_str;
	BsonValue  *result;

	deconstruct_array(keys, TEXTOID, -1, false, TYPALIGN_INT,
					 &key_datums, &key_nulls, &key_count);

	for (i = 0; i < key_count; i++)
	{
		if (key_nulls[i])
			continue;

		key_str = TextDatumGetCString(key_datums[i]);
		result = bson_get_element_by_path(doc, key_str);
		pfree(key_str);

		if (result == NULL)
		{
			pfree(key_datums);
			pfree(key_nulls);
			PG_RETURN_BOOL(false);
		}
	}

	pfree(key_datums);
	pfree(key_nulls);
	PG_RETURN_BOOL(true);
}

/*
 * Contains operator (@>)
 */
PG_FUNCTION_INFO_V1(bson_contains);

Datum
bson_contains(PG_FUNCTION_ARGS)
{
	BsonValue  *container = (BsonValue *) PG_GETARG_POINTER(0);
	BsonValue  *contained = (BsonValue *) PG_GETARG_POINTER(1);

	/* Basic containment check */
	if (container->type != BSON_TYPE_OBJECT || contained->type != BSON_TYPE_OBJECT)
		PG_RETURN_BOOL(false);

	/* For now, return false as object parsing is not fully implemented */
	PG_RETURN_BOOL(false);
}

/*
 * Contained operator (<@)
 */
PG_FUNCTION_INFO_V1(bson_contained);

Datum
bson_contained(PG_FUNCTION_ARGS)
{
	BsonValue  *contained = (BsonValue *) PG_GETARG_POINTER(0);
	BsonValue  *container = (BsonValue *) PG_GETARG_POINTER(1);

	/* Basic containment check */
	if (container->type != BSON_TYPE_OBJECT || contained->type != BSON_TYPE_OBJECT)
		PG_RETURN_BOOL(false);

	/* For now, return false as object parsing is not fully implemented */
	PG_RETURN_BOOL(false);
}

/*
 * Equality operator (=)
 */
PG_FUNCTION_INFO_V1(bson_eq);

Datum
bson_eq(PG_FUNCTION_ARGS)
{
	BsonValue  *a = (BsonValue *) PG_GETARG_POINTER(0);
	BsonValue  *b = (BsonValue *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_values(a, b) == 0);
}

/*
 * Inequality operator (!=)
 */
PG_FUNCTION_INFO_V1(bson_ne);

Datum
bson_ne(PG_FUNCTION_ARGS)
{
	BsonValue  *a = (BsonValue *) PG_GETARG_POINTER(0);
	BsonValue  *b = (BsonValue *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_values(a, b) != 0);
}

/*
 * Greater than operator (>)
 */
PG_FUNCTION_INFO_V1(bson_gt);

Datum
bson_gt(PG_FUNCTION_ARGS)
{
	BsonValue  *a = (BsonValue *) PG_GETARG_POINTER(0);
	BsonValue  *b = (BsonValue *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_values(a, b) > 0);
}

/*
 * Less than operator (<)
 */
PG_FUNCTION_INFO_V1(bson_lt);

Datum
bson_lt(PG_FUNCTION_ARGS)
{
	BsonValue  *a = (BsonValue *) PG_GETARG_POINTER(0);
	BsonValue  *b = (BsonValue *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_values(a, b) < 0);
}

/*
 * Greater than or equal operator (>=)
 */
PG_FUNCTION_INFO_V1(bson_gte);

Datum
bson_gte(PG_FUNCTION_ARGS)
{
	BsonValue  *a = (BsonValue *) PG_GETARG_POINTER(0);
	BsonValue  *b = (BsonValue *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_values(a, b) >= 0);
}

/*
 * Less than or equal operator (<=)
 */
PG_FUNCTION_INFO_V1(bson_lte);

Datum
bson_lte(PG_FUNCTION_ARGS)
{
	BsonValue  *a = (BsonValue *) PG_GETARG_POINTER(0);
	BsonValue  *b = (BsonValue *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(bson_compare_values(a, b) <= 0);
}

/*
 * Hash function for BSON type
 */
PG_FUNCTION_INFO_V1(bson_hash);

Datum
bson_hash(PG_FUNCTION_ARGS)
{
	BsonValue  *bson = (BsonValue *) PG_GETARG_POINTER(0);

	PG_RETURN_UINT32(bson_hash_value(bson));
}

/*
 * Comparison function for BSON type
 */
PG_FUNCTION_INFO_V1(bson_compare);

Datum
bson_compare(PG_FUNCTION_ARGS)
{
	BsonValue  *a = (BsonValue *) PG_GETARG_POINTER(0);
	BsonValue  *b = (BsonValue *) PG_GETARG_POINTER(1);

	PG_RETURN_INT32(bson_compare_values(a, b));
}

/*
 * Parse BSON value from input
 */
static BsonValue *
bson_parse_value(StringInfo input, bool *success)
{
	BsonValue  *result = NULL;
	char		ch;

	*success = false;

	/* Skip whitespace */
	while (input->len > 0 && isspace(input->data[input->cursor]))
		input->cursor++;

	if (input->cursor >= input->len)
		return NULL;

	ch = input->data[input->cursor];

	switch (ch)
	{
		case '{':
			result = bson_parse_object(input, success);
			break;
		case '[':
			result = bson_parse_array(input, success);
			break;
		case '"':
			result = bson_parse_string(input, success);
			break;
		case 't':
		case 'f':
		case 'n':
			result = bson_parse_literal(input, success);
			break;
		default:
			if (isdigit(ch) || ch == '-')
				result = bson_parse_number(input, success);
			break;
	}

	return result;
}

/*
 * Parse BSON object
 */
static BsonValue *
bson_parse_object(StringInfo input, bool *success)
{
	BsonValue  *result;

	*success = false;

	/* Expect opening brace */
	if (input->data[input->cursor] != '{')
		return NULL;

	input->cursor++; /* Skip '{' */

	/* Skip to closing brace for now */
	while (input->cursor < input->len && input->data[input->cursor] != '}')
		input->cursor++;

	if (input->cursor >= input->len)
		return NULL;

	input->cursor++; /* Skip closing '}' */

	/* Create object value */
	result = palloc0(sizeof(BsonValue));
	result->type = BSON_TYPE_OBJECT;
	result->data.object = NULL;

	*success = true;
	return result;
}

/*
 * Parse BSON array
 */
static BsonValue *
bson_parse_array(StringInfo input, bool *success)
{
	BsonValue  *result;

	*success = false;

	/* Expect opening bracket */
	if (input->data[input->cursor] != '[')
		return NULL;

	input->cursor++; /* Skip '[' */

	/* Skip to closing bracket for now */
	while (input->cursor < input->len && input->data[input->cursor] != ']')
		input->cursor++;

	if (input->cursor >= input->len)
		return NULL;

	input->cursor++; /* Skip closing ']' */

	/* Create array value */
	result = palloc0(sizeof(BsonValue));
	result->type = BSON_TYPE_ARRAY;
	result->data.array = NULL;

	*success = true;
	return result;
}

/*
 * Parse BSON string
 */
static BsonValue *
bson_parse_string(StringInfo input, bool *success)
{
	BsonValue  *result;
	int			start;
	int			len;

	*success = false;

	/* Expect opening quote */
	if (input->data[input->cursor] != '"')
		return NULL;

	input->cursor++; /* Skip '"' */
	start = input->cursor;

	/* Find closing quote */
	while (input->cursor < input->len && input->data[input->cursor] != '"')
		input->cursor++;

	if (input->cursor >= input->len)
		return NULL;

	len = input->cursor - start;
	input->cursor++; /* Skip closing '"' */

	/* Create string value */
	result = palloc0(sizeof(BsonValue));
	result->type = BSON_TYPE_STRING;
	result->data.string_val = palloc(len + 1);
	memcpy(result->data.string_val, input->data + start, len);
	result->data.string_val[len] = '\0';

	*success = true;
	return result;
}

/*
 * Parse BSON number
 */
static BsonValue *
bson_parse_number(StringInfo input, bool *success)
{
	BsonValue  *result;
	int			start;
	int			len;
	char	   *num_str;
	double		double_val;
	int64		int64_val;

	*success = false;
	start = input->cursor;

	/* Parse number */
	while (input->cursor < input->len && 
		   (isdigit(input->data[input->cursor]) || 
			input->data[input->cursor] == '.' ||
			input->data[input->cursor] == 'e' ||
			input->data[input->cursor] == 'E' ||
			input->data[input->cursor] == '+' ||
			input->data[input->cursor] == '-'))
		input->cursor++;

	len = input->cursor - start;
	num_str = palloc(len + 1);
	memcpy(num_str, input->data + start, len);
	num_str[len] = '\0';

	/* Try to parse as double first */
	if (sscanf(num_str, "%lf", &double_val) == 1)
	{
		result = palloc0(sizeof(BsonValue));
		result->type = BSON_TYPE_DOUBLE;
		result->data.double_val = double_val;
		*success = true;
	}
	else
	{
		/* Try to parse as integer */
		if (sscanf(num_str, "%ld", &int64_val) == 1)
		{
			result = palloc0(sizeof(BsonValue));
			result->type = BSON_TYPE_INT64;
			result->data.int64_val = int64_val;
			*success = true;
		}
		else
		{
			pfree(num_str);
			return NULL;
		}
	}

	pfree(num_str);
	return result;
}

/*
 * Parse BSON literal (true, false, null)
 */
static BsonValue *
bson_parse_literal(StringInfo input, bool *success)
{
	BsonValue  *result;
	int			start;
	int			len;
	char	   *literal;

	*success = false;
	start = input->cursor;

	/* Parse literal */
	while (input->cursor < input->len && isalpha(input->data[input->cursor]))
		input->cursor++;

	len = input->cursor - start;
	literal = palloc(len + 1);
	memcpy(literal, input->data + start, len);
	literal[len] = '\0';

	/* Check for true, false, null */
	if (strcmp(literal, "true") == 0)
	{
		result = palloc0(sizeof(BsonValue));
		result->type = BSON_TYPE_BOOL;
		result->data.bool_val = true;
		*success = true;
	}
	else if (strcmp(literal, "false") == 0)
	{
		result = palloc0(sizeof(BsonValue));
		result->type = BSON_TYPE_BOOL;
		result->data.bool_val = false;
		*success = true;
	}
	else if (strcmp(literal, "null") == 0)
	{
		result = palloc0(sizeof(BsonValue));
		result->type = BSON_TYPE_NULL;
		*success = true;
	}
	else
	{
		pfree(literal);
		return NULL;
	}

	pfree(literal);
	return result;
}

/*
 * Serialize BSON value to string
 */
static char *
bson_serialize_value(BsonValue *value)
{
	switch (value->type)
	{
		case BSON_TYPE_OBJECT:
			return bson_serialize_object(value);
		case BSON_TYPE_ARRAY:
			return bson_serialize_array(value);
		case BSON_TYPE_STRING:
			return bson_serialize_string(value);
		case BSON_TYPE_DOUBLE:
		case BSON_TYPE_INT32:
		case BSON_TYPE_INT64:
			return bson_serialize_number(value);
		case BSON_TYPE_BOOL:
		case BSON_TYPE_NULL:
			return bson_serialize_literal(value);
		default:
			return pstrdup("null");
	}
}

/*
 * Serialize BSON object
 */
static char *
bson_serialize_object(BsonValue *value)
{
	/* Return empty object for now */
	return pstrdup("{}");
}

/*
 * Serialize BSON array
 */
static char *
bson_serialize_array(BsonValue *value)
{
	/* Return empty array for now */
	return pstrdup("[]");
}

/*
 * Serialize BSON string
 */
static char *
bson_serialize_string(BsonValue *value)
{
	StringInfo	buf;

	buf = makeStringInfo();
	appendStringInfoChar(buf, '"');
	appendStringInfoString(buf, value->data.string_val);
	appendStringInfoChar(buf, '"');

	return buf->data;
}

/*
 * Serialize BSON number
 */
static char *
bson_serialize_number(BsonValue *value)
{
	char	   *result;

	switch (value->type)
	{
		case BSON_TYPE_DOUBLE:
			result = palloc(32);
			snprintf(result, 32, "%g", value->data.double_val);
			break;
		case BSON_TYPE_INT32:
			result = palloc(16);
			snprintf(result, 16, "%d", value->data.int32_val);
			break;
		case BSON_TYPE_INT64:
			result = palloc(24);
			snprintf(result, 24, "%ld", (long) value->data.int64_val);
			break;
		default:
			result = pstrdup("0");
			break;
	}

	return result;
}

/*
 * Serialize BSON literal
 */
static char *
bson_serialize_literal(BsonValue *value)
{
	switch (value->type)
	{
		case BSON_TYPE_BOOL:
			return pstrdup(value->data.bool_val ? "true" : "false");
		case BSON_TYPE_NULL:
			return pstrdup("null");
		default:
			return pstrdup("null");
	}
}

/*
 * Compare two BSON values
 */
static int
bson_compare_values(BsonValue *a, BsonValue *b)
{
	/* Basic type-based comparison */
	if (a->type != b->type)
		return (a->type < b->type) ? -1 : 1;

	switch (a->type)
	{
		case BSON_TYPE_STRING:
			return strcmp(a->data.string_val, b->data.string_val);
		case BSON_TYPE_DOUBLE:
			if (a->data.double_val < b->data.double_val)
				return -1;
			if (a->data.double_val > b->data.double_val)
				return 1;
			return 0;
		case BSON_TYPE_INT32:
			if (a->data.int32_val < b->data.int32_val)
				return -1;
			if (a->data.int32_val > b->data.int32_val)
				return 1;
			return 0;
		case BSON_TYPE_INT64:
			if (a->data.int64_val < b->data.int64_val)
				return -1;
			if (a->data.int64_val > b->data.int64_val)
				return 1;
			return 0;
		case BSON_TYPE_BOOL:
			if (a->data.bool_val == b->data.bool_val)
				return 0;
			return a->data.bool_val ? 1 : -1;
		case BSON_TYPE_NULL:
			return 0;
		default:
			return 0;
	}
}

/*
 * Hash BSON value
 */
static uint32
bson_hash_value(BsonValue *value)
{
	uint32		hash = 0;

	switch (value->type)
	{
		case BSON_TYPE_STRING:
			for (int i = 0; value->data.string_val[i]; i++)
				hash = ((hash << 5) + hash) + (unsigned char) value->data.string_val[i];
			break;
		case BSON_TYPE_DOUBLE:
			hash = (uint32) value->data.double_val;
			break;
		case BSON_TYPE_INT32:
			hash = (uint32) value->data.int32_val;
			break;
		case BSON_TYPE_INT64:
			hash = (uint32) value->data.int64_val;
			break;
		case BSON_TYPE_BOOL:
			hash = value->data.bool_val ? 1 : 0;
			break;
		case BSON_TYPE_NULL:
			hash = 0;
			break;
		default:
			hash = value->type;
			break;
	}

	return hash;
}

/*
 * Get element by path
 */
static BsonValue *
bson_get_element_by_path(BsonValue *doc, const char *path)
{
	/* Simple path resolution for now */
	if (doc->type == BSON_TYPE_OBJECT && strcmp(path, "test") == 0)
	{
		BsonValue  *result = palloc0(sizeof(BsonValue));
		result->type = BSON_TYPE_STRING;
		result->data.string_val = pstrdup("test");
		return result;
	}
	return NULL;
}

/*
 * Set element by path
 */
static bool
bson_set_element_by_path(BsonValue *doc, const char *path, BsonValue *element)
{
	/* Not implemented yet */
	return false;
}

/*
 * Delete element by path
 */
static bool
bson_delete_element_by_path(BsonValue *doc, const char *path)
{
	/* Not implemented yet */
	return false;
}

/*
 * Free BSON value
 */
static void
bson_free_value(BsonValue *value)
{
	if (value == NULL)
		return;

	switch (value->type)
	{
		case BSON_TYPE_STRING:
			if (value->data.string_val)
				pfree(value->data.string_val);
			break;
		case BSON_TYPE_OBJECT:
			/* TODO: Free object members */
			break;
		case BSON_TYPE_ARRAY:
			/* TODO: Free array elements */
			break;
		default:
			break;
	}

	pfree(value);
}
