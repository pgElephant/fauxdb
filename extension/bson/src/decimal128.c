#include "../include/decimal128.h"
#include <string.h>
#include <stdlib.h>

PG_FUNCTION_INFO_V1(decimal128_in);
Datum decimal128_in(PG_FUNCTION_ARGS) {
    char *str = PG_GETARG_CSTRING(0);
    Decimal128 *dec = (Decimal128 *) palloc(sizeof(Decimal128));
    memset(dec->data, 0, 16);
    strncpy(dec->data, str, 16); // Not a real conversion, just for demo
    PG_RETURN_POINTER(dec);
}

PG_FUNCTION_INFO_V1(decimal128_out);
Datum decimal128_out(PG_FUNCTION_ARGS) {
    Decimal128 *dec = (Decimal128 *) PG_GETARG_POINTER(0);
    char *str = (char *) palloc(33);
    for (int i = 0; i < 16; i++) {
        sprintf(str + 2*i, "%02x", (unsigned char)dec->data[i]);
    }
    str[32] = '\0';
    PG_RETURN_CSTRING(str);
}

PG_FUNCTION_INFO_V1(decimal128_eq);
Datum decimal128_eq(PG_FUNCTION_ARGS) {
    Decimal128 *a = (Decimal128 *) PG_GETARG_POINTER(0);
    Decimal128 *b = (Decimal128 *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(memcmp(a->data, b->data, 16) == 0);
}

PG_FUNCTION_INFO_V1(decimal128_ne);
Datum decimal128_ne(PG_FUNCTION_ARGS) {
    Decimal128 *a = (Decimal128 *) PG_GETARG_POINTER(0);
    Decimal128 *b = (Decimal128 *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(memcmp(a->data, b->data, 16) != 0);
}

PG_FUNCTION_INFO_V1(decimal128_hash);
Datum decimal128_hash(PG_FUNCTION_ARGS) {
    Decimal128 *dec = (Decimal128 *) PG_GETARG_POINTER(0);
    uint32 hash = 0;
    for (int i = 0; i < 16; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)dec->data[i];
    }
    PG_RETURN_INT32((int32)hash);
}

Decimal128 *decimal128_create(const char *str) {
    Decimal128 *dec = (Decimal128 *) malloc(sizeof(Decimal128));
    memset(dec->data, 0, 16);
    strncpy(dec->data, str, 16);
    return dec;
}

char *decimal128_to_string(Decimal128 *dec) {
    char *str = (char *) malloc(33);
    for (int i = 0; i < 16; i++) {
        sprintf(str + 2*i, "%02x", (unsigned char)dec->data[i]);
    }
    str[32] = '\0';
    return str;
}

bool decimal128_is_valid(const char *str) {
    return str && strlen(str) > 0;
}
