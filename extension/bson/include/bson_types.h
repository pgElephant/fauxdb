#ifndef BSON_TYPES_H
#define BSON_TYPES_H

/* Main header for all MongoDB BSON types */

/* Include all individual BSON type headers */
#include "objectid.h"
#include "binary.h"
#include "regex.h"
#include "javascript.h"
#include "decimal128.h"
#include "timestamp.h"
#include "special_keys.h"

/* MongoDB BSON Type Constants - matching MongoDB's type codes exactly */
#define BSON_TYPE_EOO		0x00	/* End of Object */
#define BSON_TYPE_DOUBLE	0x01	/* Double (64-bit floating point) */
#define BSON_TYPE_STRING	0x02	/* String (UTF-8) */
#define BSON_TYPE_OBJECT	0x03	/* Object (embedded document) */
#define BSON_TYPE_ARRAY		0x04	/* Array (list of values) */
#define BSON_TYPE_BINARY	0x05	/* Binary data */
#define BSON_TYPE_UNDEFINED	0x06	/* Undefined (deprecated) */
#define BSON_TYPE_OBJECTID	0x07	/* ObjectId (12-byte unique ID, default for _id) */
#define BSON_TYPE_BOOL		0x08	/* Boolean */
#define BSON_TYPE_DATE		0x09	/* Date (millisecond precision UTC datetime) */
#define BSON_TYPE_NULL		0x0A	/* Null */
#define BSON_TYPE_REGEX		0x0B	/* Regular Expression */
#define BSON_TYPE_DBPOINTER	0x0C	/* DBPointer (deprecated) */
#define BSON_TYPE_JAVASCRIPT	0x0D	/* JavaScript code */
#define BSON_TYPE_SYMBOL	0x0E	/* Symbol (deprecated) */
#define BSON_TYPE_JAVASCRIPT_WITH_SCOPE 0x0F	/* JavaScript (with scope) */
#define BSON_TYPE_INT32		0x10	/* 32-bit Integer */
#define BSON_TYPE_TIMESTAMP	0x11	/* Timestamp (special internal type for oplog/replication) */
#define BSON_TYPE_INT64		0x12	/* 64-bit Integer */
#define BSON_TYPE_DECIMAL128	0x13	/* Decimal128 (high-precision decimal) */
#define BSON_TYPE_MINKEY	0xFF	/* MinKey (special type that sorts lower than all others) */
#define BSON_TYPE_MAXKEY	0x7F	/* MaxKey (special type that sorts higher than all others) */

/* Main bson type - container for all BSON types */
typedef struct Bson
{
	int32		length;			/* total size in bytes */
	uint8		type;			/* BSON type */
	char		data[FLEXIBLE_ARRAY_MEMBER];
} Bson;

/* BSON element structure for parsing */
typedef struct BsonElement
{
	uint8		type;
	char	   *name;
	union
	{
		double		double_val;
		int32		int32_val;
		int64		int64_val;
		bool		bool_val;
		char	   *string_val;
		ObjectId	objectid_val;
		Binary		binary_val;
		RegEx		regex_val;
		JavaScript	javascript_val;
		JavaScriptScope javascript_scope_val;
		Decimal128	decimal128_val;
		BsonTimestamp bson_timestamp_val;
		MinKey		minkey_val;
		MaxKey		maxkey_val;
	}			value;
} BsonElement;

/* Main bson functions */
extern Datum bson_in(PG_FUNCTION_ARGS);
extern Datum bson_out(PG_FUNCTION_ARGS);
extern Datum bson_recv(PG_FUNCTION_ARGS);
extern Datum bson_send(PG_FUNCTION_ARGS);
extern Datum bson_get(PG_FUNCTION_ARGS);
extern Datum bson_get_text(PG_FUNCTION_ARGS);
extern Datum bson_set(PG_FUNCTION_ARGS);
extern Datum bson_delete(PG_FUNCTION_ARGS);
extern Datum bson_contains(PG_FUNCTION_ARGS);
extern Datum bson_contained(PG_FUNCTION_ARGS);
extern Datum bson_exists(PG_FUNCTION_ARGS);
extern Datum bson_eq(PG_FUNCTION_ARGS);
extern Datum bson_ne(PG_FUNCTION_ARGS);
extern Datum bson_gt(PG_FUNCTION_ARGS);
extern Datum bson_lt(PG_FUNCTION_ARGS);
extern Datum bson_gte(PG_FUNCTION_ARGS);
extern Datum bson_lte(PG_FUNCTION_ARGS);
extern Datum bson_in_op(PG_FUNCTION_ARGS);
extern Datum bson_nin_op(PG_FUNCTION_ARGS);
extern Datum bson_all_op(PG_FUNCTION_ARGS);
extern Datum bson_elem_match(PG_FUNCTION_ARGS);
extern Datum bson_size(PG_FUNCTION_ARGS);
extern Datum bson_type(PG_FUNCTION_ARGS);
extern Datum bson_mod(PG_FUNCTION_ARGS);
extern Datum bson_regex(PG_FUNCTION_ARGS);
extern Datum bson_hash(PG_FUNCTION_ARGS);

/* MongoDB-style query operators */
extern Datum mongo_eq(PG_FUNCTION_ARGS);
extern Datum mongo_ne(PG_FUNCTION_ARGS);
extern Datum mongo_gt(PG_FUNCTION_ARGS);
extern Datum mongo_lt(PG_FUNCTION_ARGS);
extern Datum mongo_gte(PG_FUNCTION_ARGS);
extern Datum mongo_lte(PG_FUNCTION_ARGS);
extern Datum mongo_in(PG_FUNCTION_ARGS);
extern Datum mongo_nin(PG_FUNCTION_ARGS);
extern Datum mongo_all(PG_FUNCTION_ARGS);
extern Datum mongo_elem_match(PG_FUNCTION_ARGS);
extern Datum mongo_size(PG_FUNCTION_ARGS);
extern Datum mongo_type(PG_FUNCTION_ARGS);
extern Datum mongo_mod(PG_FUNCTION_ARGS);
extern Datum mongo_regex(PG_FUNCTION_ARGS);
extern Datum mongo_exists(PG_FUNCTION_ARGS);

/* Helper functions */
extern Bson *bson_parse(const char *input);
extern char *bson_serialize(Bson *bson);
extern BsonElement *bson_get_element(Bson *bson, const char *path);
extern bool bson_set_element(Bson *bson, const char *path, BsonElement *element);
extern bool bson_delete_element(Bson *bson, const char *path);
extern int bson_compare(Bson *a, Bson *b);

#endif							/* BSON_TYPES_H */
