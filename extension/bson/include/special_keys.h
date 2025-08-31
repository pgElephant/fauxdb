#ifndef BSON_SPECIAL_KEYS_H
#define BSON_SPECIAL_KEYS_H

#include "postgres.h"
#include "fmgr.h"

/* MinKey/MaxKey - special comparison types */
typedef struct MinKey
{
	int			dummy;			/* Placeholder */
} MinKey;

typedef struct MaxKey
{
	int			dummy;			/* Placeholder */
} MaxKey;

/* MinKey functions for PostgreSQL type system */
extern Datum minkey_in(PG_FUNCTION_ARGS);
extern Datum minkey_out(PG_FUNCTION_ARGS);
extern Datum minkey_recv(PG_FUNCTION_ARGS);
extern Datum minkey_send(PG_FUNCTION_ARGS);

/* MaxKey functions for PostgreSQL type system */
extern Datum maxkey_in(PG_FUNCTION_ARGS);
extern Datum maxkey_out(PG_FUNCTION_ARGS);
extern Datum maxkey_recv(PG_FUNCTION_ARGS);
extern Datum maxkey_send(PG_FUNCTION_ARGS);

/* Comparison operators - MinKey is always less than everything */
extern Datum minkey_lt_all(PG_FUNCTION_ARGS);
extern Datum minkey_le_all(PG_FUNCTION_ARGS);

/* Comparison operators - MaxKey is always greater than everything */
extern Datum maxkey_gt_all(PG_FUNCTION_ARGS);
extern Datum maxkey_ge_all(PG_FUNCTION_ARGS);

/* Utility functions */
extern MinKey *minkey_create(void);
extern MaxKey *maxkey_create(void);
extern bool is_minkey(Datum value);
extern bool is_maxkey(Datum value);

#endif							/* BSON_SPECIAL_KEYS_H */
