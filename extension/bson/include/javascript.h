#ifndef BSON_JAVASCRIPT_H
#define BSON_JAVASCRIPT_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/json.h"

/* JavaScript code */
typedef struct JavaScript
{
	char	   *code;			/* JavaScript code string */
} JavaScript;

/* JavaScript with scope */
typedef struct JavaScriptScope
{
	char	   *code;			/* JavaScript code string */
	char	   *scope;			/* Scope document */
} JavaScriptScope;

/* JavaScript functions for PostgreSQL type system */
extern Datum javascript_in(PG_FUNCTION_ARGS);
extern Datum javascript_out(PG_FUNCTION_ARGS);
extern Datum javascript_recv(PG_FUNCTION_ARGS);
extern Datum javascript_send(PG_FUNCTION_ARGS);

/* JavaScriptScope functions */
extern Datum javascript_scope_in(PG_FUNCTION_ARGS);
extern Datum javascript_scope_out(PG_FUNCTION_ARGS);
extern Datum javascript_scope_recv(PG_FUNCTION_ARGS);
extern Datum javascript_scope_send(PG_FUNCTION_ARGS);

/* Utility functions */
extern JavaScript *javascript_create(const char *code);
extern JavaScriptScope *javascript_scope_create(const char *code, const char *scope);
extern char *javascript_get_code(JavaScript *js);
extern char *javascript_scope_get_code(JavaScriptScope *js);
extern char *javascript_scope_get_scope(JavaScriptScope *js);
extern bool javascript_is_valid(const char *code);

#endif							/* BSON_JAVASCRIPT_H */
