#ifndef BSON_BINARY_H
#define BSON_BINARY_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/bytea.h"

/* Binary data - arbitrary byte array */
typedef struct Binary
{
	int32		length;
	uint8		subtype;		/* Binary subtype */
	char		data[FLEXIBLE_ARRAY_MEMBER];
} Binary;

/* Binary subtypes matching MongoDB */
#define BINARY_SUBTYPE_GENERIC		0x00
#define BINARY_SUBTYPE_FUNCTION		0x01
#define BINARY_SUBTYPE_BINARY_OLD	0x02
#define BINARY_SUBTYPE_UUID_OLD		0x03
#define BINARY_SUBTYPE_UUID			0x04
#define BINARY_SUBTYPE_MD5			0x05
#define BINARY_SUBTYPE_ENCRYPTED	0x06
#define BINARY_SUBTYPE_COMPRESSED	0x07
#define BINARY_SUBTYPE_USER_DEFINED	0x80

/* Binary functions for PostgreSQL type system */
extern Datum binary_in(PG_FUNCTION_ARGS);
extern Datum binary_out(PG_FUNCTION_ARGS);
extern Datum binary_recv(PG_FUNCTION_ARGS);
extern Datum binary_send(PG_FUNCTION_ARGS);

/* Comparison operators */
extern Datum binary_eq(PG_FUNCTION_ARGS);
extern Datum binary_ne(PG_FUNCTION_ARGS);

/* Hash function for indexing */
extern Datum binary_hash(PG_FUNCTION_ARGS);

/* Utility functions */
extern Binary *binary_create(const char *data, int32 length, uint8 subtype);
extern char *binary_get_data(Binary *bin);
extern int32 binary_get_length(Binary *bin);
extern uint8 binary_get_subtype(Binary *bin);
extern bool binary_is_subtype(Binary *bin, uint8 subtype);

#endif							/* BSON_BINARY_H */
