/*
 * bson_agg.c
 *		  PostgreSQL BSON extension - Aggregation functions
 *
 * This module implements document-style aggregation functions for BSON
 * documents using the official libbson library for all BSON operations.
 *
 * Provides aggregation pipeline functionality similar to document database
 * $group, $match, $project, etc. operators for seamless document-to-
 * PostgreSQL migration.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  extension/bson/src/bson_agg.c
 */

#include "access/hash.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "nodes/execnodes.h"
#include "postgres.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

/* Official libbson - use ONLY official libbson library */
#include <bson/bson.h>

/*
 * Helper structures and functions
 */
typedef struct BsonData
{
    int32 vl_len_; /* varlena header (do not touch directly!) */
    char data[FLEXIBLE_ARRAY_MEMBER];
} BsonData;

/*
 * bson_data_to_bson_t
 *		Convert BsonData to bson_t structure
 */
static bson_t* bson_data_to_bson_t(const BsonData* bson_data)
{
    const uint8_t* data = (const uint8_t*)bson_data->data;
    size_t len = VARSIZE_ANY_EXHDR(bson_data);

    return bson_new_from_data(data, len);
}

/*
 * bson_t_to_bson_data
 *		Convert bson_t structure to BsonData
 */
static BsonData* bson_t_to_bson_data(const bson_t* bson_doc)
{
    BsonData* result;
    const uint8_t* data;
    uint32_t len;

    data = bson_get_data(bson_doc);
    len = bson_doc->len;

    result = (BsonData*)palloc(VARHDRSZ + len);
    SET_VARSIZE(result, VARHDRSZ + len);
    memcpy(result->data, data, len);

    return result;
}

/*
 * BSON aggregation state for aggregate functions
 */
typedef struct BsonAggState
{
    bson_t* result_bson;       /* accumulated BSON document */
    MemoryContext agg_context; /* memory context for aggregation */
    int count;                 /* number of processed items */
    bool first_call;           /* true if this is the first call */
} BsonAggState;

/*
 * bson_agg_transfn
 *		Transition function for BSON aggregation
 *
 * Equivalent to document database $group operator - collects BSON documents
 * into a single aggregate document.
 */
PG_FUNCTION_INFO_V1(bson_agg_transfn);
Datum bson_agg_transfn(PG_FUNCTION_ARGS)
{
    MemoryContext old_context;
    MemoryContext agg_context;
    BsonAggState* state;

    if (!AggCheckCallContext(fcinfo, &agg_context))
        elog(ERROR, "bson_agg_transfn called in non-aggregate context");

    old_context = MemoryContextSwitchTo(agg_context);

    if (PG_ARGISNULL(0))
    {
        state = (BsonAggState*)palloc0(sizeof(BsonAggState));
        state->result_bson = bson_new();
        state->agg_context = agg_context;
        state->count = 0;
        state->first_call = true;
    }
    else
    {
        state = (BsonAggState*)PG_GETARG_POINTER(0);
    }

    if (!PG_ARGISNULL(1))
    {
        BsonData* input_data = (BsonData*)PG_GETARG_POINTER(1);
        bson_t* input_bson = bson_data_to_bson_t(input_data);

        if (input_bson != NULL)
        {
            char key[32];
            bson_iter_t iter;

            snprintf(key, sizeof(key), "%d", state->count++);

            /* Convert input BSON to a subdocument in result array */
            if (bson_iter_init(&iter, input_bson))
            {
                bson_t* temp_doc = bson_new();

                while (bson_iter_next(&iter))
                {
                    const char* field_key = bson_iter_key(&iter);
                    const bson_value_t* value = bson_iter_value(&iter);

                    bson_append_value(temp_doc, field_key, -1, value);
                }

                bson_append_document(state->result_bson, key, -1, temp_doc);
                bson_destroy(temp_doc);
            }

            bson_destroy(input_bson);
        }
    }

    MemoryContextSwitchTo(old_context);
    PG_RETURN_POINTER(state);
}

PG_FUNCTION_INFO_V1(bson_agg_finalfn);
Datum bson_agg_finalfn(PG_FUNCTION_ARGS)
{
    BsonAggState* state;
    BsonData* result;

    if (PG_ARGISNULL(0))
    {
        bson_t* empty_array = bson_new();
        result = bson_t_to_bson_data(empty_array);
        bson_destroy(empty_array);
        PG_RETURN_POINTER(result);
    }

    state = (BsonAggState*)PG_GETARG_POINTER(0);
    result = bson_t_to_bson_data(state->result_bson);

    PG_RETURN_POINTER(result);
}

/*
 * Document $push equivalent - Build BSON array from values
 */
PG_FUNCTION_INFO_V1(bson_array_agg_transfn);
Datum bson_array_agg_transfn(PG_FUNCTION_ARGS)
{
    MemoryContext old_context;
    MemoryContext agg_context;
    BsonAggState* state;

    if (!AggCheckCallContext(fcinfo, &agg_context))
        elog(ERROR, "bson_array_agg_transfn called in non-aggregate context");

    old_context = MemoryContextSwitchTo(agg_context);

    if (PG_ARGISNULL(0))
    {
        state = (BsonAggState*)palloc0(sizeof(BsonAggState));
        state->result_bson = bson_new();
        state->agg_context = agg_context;
        state->count = 0;
        state->first_call = true;
    }
    else
    {
        state = (BsonAggState*)PG_GETARG_POINTER(0);
    }

    if (!PG_ARGISNULL(1))
    {
        Datum input = PG_GETARG_DATUM(1);
        Oid input_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
        char key[32];

        snprintf(key, sizeof(key), "%d", state->count++);

        switch (input_type)
        {
        case TEXTOID:
        {
            text* txt = DatumGetTextP(input);
            char* str = text_to_cstring(txt);
            bson_append_utf8(state->result_bson, key, -1, str, -1);
            pfree(str);
            break;
        }
        case INT4OID:
        {
            int32 val = DatumGetInt32(input);
            bson_append_int32(state->result_bson, key, -1, val);
            break;
        }
        case INT8OID:
        {
            int64 val = DatumGetInt64(input);
            bson_append_int64(state->result_bson, key, -1, val);
            break;
        }
        case FLOAT8OID:
        {
            double val = DatumGetFloat8(input);
            bson_append_double(state->result_bson, key, -1, val);
            break;
        }
        case BOOLOID:
        {
            bool val = DatumGetBool(input);
            bson_append_bool(state->result_bson, key, -1, val);
            break;
        }
        default:
        {
            /* For other types, try to convert to string representation */
            Oid output_func;
            bool is_varlena;
            char* str;

            getTypeOutputInfo(input_type, &output_func, &is_varlena);
            str = OidOutputFunctionCall(output_func, input);
            bson_append_utf8(state->result_bson, key, -1, str, -1);
            pfree(str);
            break;
        }
        }
    }

    MemoryContextSwitchTo(old_context);
    PG_RETURN_POINTER(state);
}

PG_FUNCTION_INFO_V1(bson_array_agg_finalfn);
Datum bson_array_agg_finalfn(PG_FUNCTION_ARGS)
{
    BsonAggState* state;
    BsonData* result;

    if (PG_ARGISNULL(0))
    {
        bson_t* empty_array = bson_new();
        result = bson_t_to_bson_data(empty_array);
        bson_destroy(empty_array);
        PG_RETURN_POINTER(result);
    }

    state = (BsonAggState*)PG_GETARG_POINTER(0);

    /* Convert document to array format */
    bson_t* array_bson = bson_new();
    bson_iter_t iter;

    if (bson_iter_init(&iter, state->result_bson))
    {
        while (bson_iter_next(&iter))
        {
            const char* key = bson_iter_key(&iter);
            const bson_value_t* value = bson_iter_value(&iter);
            bson_append_value(array_bson, key, -1, value);
        }
    }

    result = bson_t_to_bson_data(array_bson);
    bson_destroy(array_bson);

    PG_RETURN_POINTER(result);
}

/*
 * BSON field extraction for aggregation
 */
PG_FUNCTION_INFO_V1(bson_extract_field);
Datum bson_extract_field(PG_FUNCTION_ARGS)
{
    BsonData* bson_data = (BsonData*)PG_GETARG_POINTER(0);
    text* field = PG_GETARG_TEXT_P(1);
    char* field_str = text_to_cstring(field);
    bson_t* bson;
    bson_iter_t iter;
    Datum result;
    bool is_null = true;

    bson = bson_data_to_bson_t(bson_data);
    if (!bson)
    {
        pfree(field_str);
        PG_RETURN_NULL();
    }

    if (bson_iter_init_find(&iter, bson, field_str))
    {
        bson_type_t type = bson_iter_type(&iter);

        switch (type)
        {
        case BSON_TYPE_UTF8:
        {
            const char* str_value = bson_iter_utf8(&iter, NULL);
            result = CStringGetTextDatum(str_value);
            is_null = false;
            break;
        }
        case BSON_TYPE_INT32:
        {
            int32 int_value = bson_iter_int32(&iter);
            result = Int32GetDatum(int_value);
            is_null = false;
            break;
        }
        case BSON_TYPE_INT64:
        {
            int64 int64_value = bson_iter_int64(&iter);
            result = Int64GetDatum(int64_value);
            is_null = false;
            break;
        }
        case BSON_TYPE_DOUBLE:
        {
            double double_value = bson_iter_double(&iter);
            result = Float8GetDatum(double_value);
            is_null = false;
            break;
        }
        case BSON_TYPE_BOOL:
        {
            bool bool_value = bson_iter_bool(&iter);
            result = BoolGetDatum(bool_value);
            is_null = false;
            break;
        }
        case BSON_TYPE_NULL:
        {
            result = (Datum)0;
            is_null = true;
            break;
        }
        default:
        {
            /* For complex types, return as BSON subdocument */
            bson_t* sub_bson = bson_new();
            const bson_value_t* value = bson_iter_value(&iter);
            bson_append_value(sub_bson, "value", -1, value);

            BsonData* sub_result = bson_t_to_bson_data(sub_bson);
            result = PointerGetDatum(sub_result);
            is_null = false;

            bson_destroy(sub_bson);
            break;
        }
        }
    }
    else
    {
        result = (Datum)0;
        is_null = true;
    }

    bson_destroy(bson);
    pfree(field_str);

    if (is_null)
        PG_RETURN_NULL();
    else
        PG_RETURN_DATUM(result);
}

/*
 * BSON merge function - Combine multiple BSON documents
 */
PG_FUNCTION_INFO_V1(bson_merge);
Datum bson_merge(PG_FUNCTION_ARGS)
{
    bytea* arg1 = PG_GETARG_BYTEA_PP(0);
    bytea* arg2 = PG_GETARG_BYTEA_PP(1);
    bson_t bson1, bson2, result;
    bson_iter_t iter;
    const char* key;
    const bson_value_t* value;
    bytea* output;
    const uint8_t* data;
    uint32_t length;

    /* Initialize BSON documents */
    if (!bson_init_static(&bson1, (const uint8_t*)VARDATA_ANY(arg1),
                          VARSIZE_ANY_EXHDR(arg1)))
    {
        ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED),
                        errmsg("invalid BSON data in first argument")));
    }

    if (!bson_init_static(&bson2, (const uint8_t*)VARDATA_ANY(arg2),
                          VARSIZE_ANY_EXHDR(arg2)))
    {
        ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED),
                        errmsg("invalid BSON data in second argument")));
    }

    /* Initialize result BSON */
    bson_init(&result);

    /* Copy all fields from first document */
    if (bson_iter_init(&iter, &bson1))
    {
        while (bson_iter_next(&iter))
        {
            key = bson_iter_key(&iter);
            value = bson_iter_value(&iter);
            bson_append_value(&result, key, -1, value);
        }
    }

    /* Copy fields from second document (overwrites duplicates) */
    if (bson_iter_init(&iter, &bson2))
    {
        while (bson_iter_next(&iter))
        {
            key = bson_iter_key(&iter);
            value = bson_iter_value(&iter);

            /* Check if key exists in result and remove it */
            bson_iter_t result_iter;
            bool found = false;
            if (bson_iter_init(&result_iter, &result))
            {
                while (bson_iter_next(&result_iter))
                {
                    if (strcmp(bson_iter_key(&result_iter), key) == 0)
                    {
                        found = true;
                        break;
                    }
                }
            }

            if (found)
            {
                /* Need to rebuild document without the duplicate key */
                bson_t temp_result;
                bson_init(&temp_result);

                bson_iter_init(&result_iter, &result);
                while (bson_iter_next(&result_iter))
                {
                    const char* existing_key = bson_iter_key(&result_iter);
                    if (strcmp(existing_key, key) != 0)
                    {
                        const bson_value_t* existing_value =
                            bson_iter_value(&result_iter);
                        bson_append_value(&temp_result, existing_key, -1,
                                          existing_value);
                    }
                }

                /* Add the new field */
                bson_append_value(&temp_result, key, -1, value);

                /* Replace result with temp_result */
                bson_destroy(&result);
                result = temp_result;
            }
            else
            {
                /* Key doesn't exist, just append */
                bson_append_value(&result, key, -1, value);
            }
        }
    }

    /* Get result data */
    data = bson_get_data(&result);
    length = result.len;

    /* Create output bytea */
    output = (bytea*)palloc(VARHDRSZ + length);
    SET_VARSIZE(output, VARHDRSZ + length);
    memcpy(VARDATA(output), data, length);

    bson_destroy(&result);

    PG_RETURN_BYTEA_P(output);
}

/*
 * BSON count aggregation - Count non-null field values
 */
PG_FUNCTION_INFO_V1(bson_count_field);
Datum bson_count_field(PG_FUNCTION_ARGS)
{
    BsonData* bson_data = (BsonData*)PG_GETARG_POINTER(0);
    text* field = PG_GETARG_TEXT_P(1);
    char* field_str = text_to_cstring(field);
    bson_t* bson;
    bson_iter_t iter;
    int32 count = 0;

    bson = bson_data_to_bson_t(bson_data);
    if (!bson)
    {
        pfree(field_str);
        PG_RETURN_INT32(0);
    }

    if (bson_iter_init_find(&iter, bson, field_str))
    {
        bson_type_t type = bson_iter_type(&iter);

        if (type == BSON_TYPE_ARRAY)
        {
            bson_iter_t array_iter;
            uint32_t array_len;
            const uint8_t* array_data;

            bson_iter_array(&iter, &array_len, &array_data);
            bson_t* array_bson = bson_new_from_data(array_data, array_len);

            if (array_bson)
            {
                bson_iter_init(&array_iter, array_bson);

                while (bson_iter_next(&array_iter))
                {
                    if (bson_iter_type(&array_iter) != BSON_TYPE_NULL)
                        count++;
                }

                bson_destroy(array_bson);
            }
        }
        else if (type != BSON_TYPE_NULL)
        {
            count = 1;
        }
    }

    bson_destroy(bson);
    pfree(field_str);

    PG_RETURN_INT32(count);
}

/*
 * BSON sum aggregation - Sum numeric field values
 */
PG_FUNCTION_INFO_V1(bson_sum_field);
Datum bson_sum_field(PG_FUNCTION_ARGS)
{
    BsonData* bson_data = (BsonData*)PG_GETARG_POINTER(0);
    text* field = PG_GETARG_TEXT_P(1);
    char* field_str = text_to_cstring(field);
    bson_t* bson;
    bson_iter_t iter;
    double sum = 0.0;
    bool found = false;

    bson = bson_data_to_bson_t(bson_data);
    if (!bson)
    {
        pfree(field_str);
        PG_RETURN_NULL();
    }

    if (bson_iter_init_find(&iter, bson, field_str))
    {
        bson_type_t type = bson_iter_type(&iter);

        switch (type)
        {
        case BSON_TYPE_INT32:
            sum = (double)bson_iter_int32(&iter);
            found = true;
            break;
        case BSON_TYPE_INT64:
            sum = (double)bson_iter_int64(&iter);
            found = true;
            break;
        case BSON_TYPE_DOUBLE:
            sum = bson_iter_double(&iter);
            found = true;
            break;
        case BSON_TYPE_ARRAY:
        {
            bson_iter_t array_iter;
            uint32_t array_len;
            const uint8_t* array_data;

            bson_iter_array(&iter, &array_len, &array_data);
            bson_t* array_bson = bson_new_from_data(array_data, array_len);

            if (array_bson)
            {
                bson_iter_init(&array_iter, array_bson);

                while (bson_iter_next(&array_iter))
                {
                    bson_type_t array_type = bson_iter_type(&array_iter);

                    switch (array_type)
                    {
                    case BSON_TYPE_INT32:
                        sum += (double)bson_iter_int32(&array_iter);
                        found = true;
                        break;
                    case BSON_TYPE_INT64:
                        sum += (double)bson_iter_int64(&array_iter);
                        found = true;
                        break;
                    case BSON_TYPE_DOUBLE:
                        sum += bson_iter_double(&array_iter);
                        found = true;
                        break;
                    default:
                        break;
                    }
                }

                bson_destroy(array_bson);
            }
            break;
        }
        default:
            break;
        }
    }

    bson_destroy(bson);
    pfree(field_str);

    if (found)
        PG_RETURN_FLOAT8(sum);
    else
        PG_RETURN_NULL();
}

/*
 * BSON average aggregation - Average of numeric field values
 */
PG_FUNCTION_INFO_V1(bson_avg_field);
Datum bson_avg_field(PG_FUNCTION_ARGS)
{
    BsonData* bson_data = (BsonData*)PG_GETARG_POINTER(0);
    text* field = PG_GETARG_TEXT_P(1);
    char* field_str = text_to_cstring(field);
    bson_t* bson;
    bson_iter_t iter;
    double sum = 0.0;
    int32 count = 0;

    bson = bson_data_to_bson_t(bson_data);
    if (!bson)
    {
        pfree(field_str);
        PG_RETURN_NULL();
    }

    if (bson_iter_init_find(&iter, bson, field_str))
    {
        bson_type_t type = bson_iter_type(&iter);

        switch (type)
        {
        case BSON_TYPE_INT32:
            sum = (double)bson_iter_int32(&iter);
            count = 1;
            break;
        case BSON_TYPE_INT64:
            sum = (double)bson_iter_int64(&iter);
            count = 1;
            break;
        case BSON_TYPE_DOUBLE:
            sum = bson_iter_double(&iter);
            count = 1;
            break;
        case BSON_TYPE_ARRAY:
        {
            bson_iter_t array_iter;
            uint32_t array_len;
            const uint8_t* array_data;

            bson_iter_array(&iter, &array_len, &array_data);
            bson_t* array_bson = bson_new_from_data(array_data, array_len);

            if (array_bson)
            {
                bson_iter_init(&array_iter, array_bson);

                while (bson_iter_next(&array_iter))
                {
                    bson_type_t array_type = bson_iter_type(&array_iter);

                    switch (array_type)
                    {
                    case BSON_TYPE_INT32:
                        sum += (double)bson_iter_int32(&array_iter);
                        count++;
                        break;
                    case BSON_TYPE_INT64:
                        sum += (double)bson_iter_int64(&array_iter);
                        count++;
                        break;
                    case BSON_TYPE_DOUBLE:
                        sum += bson_iter_double(&array_iter);
                        count++;
                        break;
                    default:
                        break;
                    }
                }

                bson_destroy(array_bson);
            }
            break;
        }
        default:
            break;
        }
    }

    bson_destroy(bson);
    pfree(field_str);

    if (count > 0)
        PG_RETURN_FLOAT8(sum / count);
    else
        PG_RETURN_NULL();
}

/*
 * BSON min/max aggregation - Find minimum/maximum field values
 */
PG_FUNCTION_INFO_V1(bson_min_field);
Datum bson_min_field(PG_FUNCTION_ARGS)
{
    BsonData* bson_data = (BsonData*)PG_GETARG_POINTER(0);
    text* field = PG_GETARG_TEXT_P(1);
    char* field_str = text_to_cstring(field);
    bson_t* bson;
    bson_iter_t iter;
    double min_value = 0.0;
    bool found = false;

    bson = bson_data_to_bson_t(bson_data);
    if (!bson)
    {
        pfree(field_str);
        PG_RETURN_NULL();
    }

    if (bson_iter_init_find(&iter, bson, field_str))
    {
        bson_type_t type = bson_iter_type(&iter);

        switch (type)
        {
        case BSON_TYPE_INT32:
            min_value = (double)bson_iter_int32(&iter);
            found = true;
            break;
        case BSON_TYPE_INT64:
            min_value = (double)bson_iter_int64(&iter);
            found = true;
            break;
        case BSON_TYPE_DOUBLE:
            min_value = bson_iter_double(&iter);
            found = true;
            break;
        case BSON_TYPE_ARRAY:
        {
            bson_iter_t array_iter;
            uint32_t array_len;
            const uint8_t* array_data;

            bson_iter_array(&iter, &array_len, &array_data);
            bson_t* array_bson = bson_new_from_data(array_data, array_len);

            if (array_bson)
            {
                bson_iter_init(&array_iter, array_bson);

                while (bson_iter_next(&array_iter))
                {
                    bson_type_t array_type = bson_iter_type(&array_iter);
                    double current_value = 0.0;
                    bool current_found = false;

                    switch (array_type)
                    {
                    case BSON_TYPE_INT32:
                        current_value = (double)bson_iter_int32(&array_iter);
                        current_found = true;
                        break;
                    case BSON_TYPE_INT64:
                        current_value = (double)bson_iter_int64(&array_iter);
                        current_found = true;
                        break;
                    case BSON_TYPE_DOUBLE:
                        current_value = bson_iter_double(&array_iter);
                        current_found = true;
                        break;
                    default:
                        break;
                    }

                    if (current_found)
                    {
                        if (!found || current_value < min_value)
                            min_value = current_value;
                        found = true;
                    }
                }

                bson_destroy(array_bson);
            }
            break;
        }
        default:
            break;
        }
    }

    bson_destroy(bson);
    pfree(field_str);

    if (found)
        PG_RETURN_FLOAT8(min_value);
    else
        PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(bson_max_field);
Datum bson_max_field(PG_FUNCTION_ARGS)
{
    BsonData* bson_data = (BsonData*)PG_GETARG_POINTER(0);
    text* field = PG_GETARG_TEXT_P(1);
    char* field_str = text_to_cstring(field);
    bson_t* bson;
    bson_iter_t iter;
    double max_value = 0.0;
    bool found = false;

    bson = bson_data_to_bson_t(bson_data);
    if (!bson)
    {
        pfree(field_str);
        PG_RETURN_NULL();
    }

    if (bson_iter_init_find(&iter, bson, field_str))
    {
        bson_type_t type = bson_iter_type(&iter);

        switch (type)
        {
        case BSON_TYPE_INT32:
            max_value = (double)bson_iter_int32(&iter);
            found = true;
            break;
        case BSON_TYPE_INT64:
            max_value = (double)bson_iter_int64(&iter);
            found = true;
            break;
        case BSON_TYPE_DOUBLE:
            max_value = bson_iter_double(&iter);
            found = true;
            break;
        case BSON_TYPE_ARRAY:
        {
            bson_iter_t array_iter;
            uint32_t array_len;
            const uint8_t* array_data;

            bson_iter_array(&iter, &array_len, &array_data);
            bson_t* array_bson = bson_new_from_data(array_data, array_len);

            if (array_bson)
            {
                bson_iter_init(&array_iter, array_bson);

                while (bson_iter_next(&array_iter))
                {
                    bson_type_t array_type = bson_iter_type(&array_iter);
                    double current_value = 0.0;
                    bool current_found = false;

                    switch (array_type)
                    {
                    case BSON_TYPE_INT32:
                        current_value = (double)bson_iter_int32(&array_iter);
                        current_found = true;
                        break;
                    case BSON_TYPE_INT64:
                        current_value = (double)bson_iter_int64(&array_iter);
                        current_found = true;
                        break;
                    case BSON_TYPE_DOUBLE:
                        current_value = bson_iter_double(&array_iter);
                        current_found = true;
                        break;
                    default:
                        break;
                    }

                    if (current_found)
                    {
                        if (!found || current_value > max_value)
                            max_value = current_value;
                        found = true;
                    }
                }

                bson_destroy(array_bson);
            }
            break;
        }
        default:
            break;
        }
    }

    bson_destroy(bson);
    pfree(field_str);

    if (found)
        PG_RETURN_FLOAT8(max_value);
    else
        PG_RETURN_NULL();
}
