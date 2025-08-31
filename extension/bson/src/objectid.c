#include "../include/objectid.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Implementation for ObjectId functions

PG_FUNCTION_INFO_V1(objectid_in);
Datum objectid_in(PG_FUNCTION_ARGS) {
    char *str = PG_GETARG_CSTRING(0);
    ObjectId *oid = (ObjectId *) palloc(sizeof(ObjectId));
    if (strlen(str) == 24) {
        // Convert hex string to 12-byte ObjectId
        for (int i = 0; i < 12; i++) {
            sscanf(str + 2*i, "%2hhx", &oid->data[i]);
        }
    } else {
        memset(oid->data, 0, 12);
    }
    PG_RETURN_POINTER(oid);
}

PG_FUNCTION_INFO_V1(objectid_out);
Datum objectid_out(PG_FUNCTION_ARGS) {
    ObjectId *oid = (ObjectId *) PG_GETARG_POINTER(0);
    char *str = (char *) palloc(25);
    for (int i = 0; i < 12; i++) {
        sprintf(str + 2*i, "%02x", (unsigned char)oid->data[i]);
    }
    str[24] = '\0';
    PG_RETURN_CSTRING(str);
}

PG_FUNCTION_INFO_V1(objectid_eq);
Datum objectid_eq(PG_FUNCTION_ARGS) {
    ObjectId *a = (ObjectId *) PG_GETARG_POINTER(0);
    ObjectId *b = (ObjectId *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(memcmp(a->data, b->data, 12) == 0);
}

PG_FUNCTION_INFO_V1(objectid_ne);
Datum objectid_ne(PG_FUNCTION_ARGS) {
    ObjectId *a = (ObjectId *) PG_GETARG_POINTER(0);
    ObjectId *b = (ObjectId *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(memcmp(a->data, b->data, 12) != 0);
}

PG_FUNCTION_INFO_V1(objectid_lt);
Datum objectid_lt(PG_FUNCTION_ARGS) {
    ObjectId *a = (ObjectId *) PG_GETARG_POINTER(0);
    ObjectId *b = (ObjectId *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(memcmp(a->data, b->data, 12) < 0);
}

PG_FUNCTION_INFO_V1(objectid_gt);
Datum objectid_gt(PG_FUNCTION_ARGS) {
    ObjectId *a = (ObjectId *) PG_GETARG_POINTER(0);
    ObjectId *b = (ObjectId *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(memcmp(a->data, b->data, 12) > 0);
}

PG_FUNCTION_INFO_V1(objectid_le);
Datum objectid_le(PG_FUNCTION_ARGS) {
    ObjectId *a = (ObjectId *) PG_GETARG_POINTER(0);
    ObjectId *b = (ObjectId *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(memcmp(a->data, b->data, 12) <= 0);
}

PG_FUNCTION_INFO_V1(objectid_ge);
Datum objectid_ge(PG_FUNCTION_ARGS) {
    ObjectId *a = (ObjectId *) PG_GETARG_POINTER(0);
    ObjectId *b = (ObjectId *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(memcmp(a->data, b->data, 12) >= 0);
}

PG_FUNCTION_INFO_V1(objectid_hash);
Datum objectid_hash(PG_FUNCTION_ARGS) {
    ObjectId *oid = (ObjectId *) PG_GETARG_POINTER(0);
    uint32 hash = 0;
    for (int i = 0; i < 12; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)oid->data[i];
    }
    PG_RETURN_INT32((int32)hash);
}

ObjectId *objectid_generate(void) {
    ObjectId *oid = (ObjectId *) malloc(sizeof(ObjectId));
    for (int i = 0; i < 12; i++) {
        oid->data[i] = (char)(rand() % 256);
    }
    return oid;
}

char *objectid_to_string(ObjectId *oid) {
    char *str = (char *) malloc(25);
    for (int i = 0; i < 12; i++) {
        sprintf(str + 2*i, "%02x", (unsigned char)oid->data[i]);
    }
    str[24] = '\0';
    return str;
}

ObjectId *string_to_objectid(const char *str) {
    ObjectId *oid = (ObjectId *) malloc(sizeof(ObjectId));
    if (strlen(str) == 24) {
        for (int i = 0; i < 12; i++) {
            sscanf(str + 2*i, "%2hhx", &oid->data[i]);
        }
    } else {
        memset(oid->data, 0, 12);
    }
    return oid;
}

bool objectid_is_valid(const char *str) {
    if (!str) return false;
    if (strlen(str) != 24) return false;
    for (int i = 0; i < 24; i++) {
        if (!((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'f') || (str[i] >= 'A' && str[i] <= 'F')))
            return false;
    }
    return true;
}
