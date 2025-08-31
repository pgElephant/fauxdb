#include "../include/binary.h"
#include "postgres.h"
#include "fmgr.h"
#include "utils/bytea.h"
#include "varatt.h"
#include <string.h>
#include <stdlib.h>

// Implementation for binary functions

PG_FUNCTION_INFO_V1(binary_in);
Datum binary_in(PG_FUNCTION_ARGS) {
    bytea *input = PG_GETARG_BYTEA_P(0);
    int32 len = VARSIZE(input) - VARHDRSZ;
    Binary *bin = (Binary *) palloc(sizeof(Binary) + len);
    bin->length = len;
    bin->subtype = BINARY_SUBTYPE_GENERIC;
    memcpy(bin->data, VARDATA(input), len);
    PG_RETURN_POINTER(bin);
}

PG_FUNCTION_INFO_V1(binary_out);
Datum binary_out(PG_FUNCTION_ARGS) {
    Binary *bin = (Binary *) PG_GETARG_POINTER(0);
    bytea *result = (bytea *) palloc(VARHDRSZ + bin->length);
    SET_VARSIZE(result, VARHDRSZ + bin->length);
    memcpy(VARDATA(result), bin->data, bin->length);
    PG_RETURN_BYTEA_P(result);
}

PG_FUNCTION_INFO_V1(binary_eq);
Datum binary_eq(PG_FUNCTION_ARGS) {
    Binary *a = (Binary *) PG_GETARG_POINTER(0);
    Binary *b = (Binary *) PG_GETARG_POINTER(1);
    if (a->length != b->length) PG_RETURN_BOOL(false);
    PG_RETURN_BOOL(memcmp(a->data, b->data, a->length) == 0);
}

PG_FUNCTION_INFO_V1(binary_ne);
Datum binary_ne(PG_FUNCTION_ARGS) {
    Binary *a = (Binary *) PG_GETARG_POINTER(0);
    Binary *b = (Binary *) PG_GETARG_POINTER(1);
    if (a->length != b->length) PG_RETURN_BOOL(true);
    PG_RETURN_BOOL(memcmp(a->data, b->data, a->length) != 0);
}

PG_FUNCTION_INFO_V1(binary_hash);
Datum binary_hash(PG_FUNCTION_ARGS) {
    Binary *bin = (Binary *) PG_GETARG_POINTER(0);
    uint32 hash = 0;
    for (int i = 0; i < bin->length; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)bin->data[i];
    }
    PG_RETURN_INT32((int32)hash);
}

Binary *binary_create(const char *data, int32 length, uint8 subtype) {
    Binary *bin = (Binary *) malloc(sizeof(Binary) + length);
    bin->length = length;
    bin->subtype = subtype;
    memcpy(bin->data, data, length);
    return bin;
}

char *binary_get_data(Binary *bin) {
    return bin->data;
}

int32 binary_get_length(Binary *bin) {
    return bin->length;
}

uint8 binary_get_subtype(Binary *bin) {
    return bin->subtype;
}

bool binary_is_subtype(Binary *bin, uint8 subtype) {
    return bin->subtype == subtype;
}
