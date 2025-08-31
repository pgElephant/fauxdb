/*
 * regex.h
 *
 * PostgreSQL BSON extension - RegEx type definitions
 *
 * This header defines the RegEx type and related functions for MongoDB-compatible
 * regular expression support in the BSON extension.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    contrib/bson/regex.h
 */

#ifndef BSON_REGEX_H
#define BSON_REGEX_H

#include "postgres.h"
#include "fmgr.h"

/*
 * Regular Expression structure
 * Compatible with MongoDB BSON RegEx type
 */
typedef struct RegEx
{
	char	   *pattern;		/* Regex pattern */
	char	   *options;		/* Regex options (i, m, s, x) */
} RegEx;

/*
 * RegEx type I/O functions for PostgreSQL type system
 */
extern Datum regex_in(PG_FUNCTION_ARGS);
extern Datum regex_out(PG_FUNCTION_ARGS);
extern Datum regex_recv(PG_FUNCTION_ARGS);
extern Datum regex_send(PG_FUNCTION_ARGS);

/*
 * Comparison operators
 */
extern Datum regex_eq(PG_FUNCTION_ARGS);
extern Datum regex_ne(PG_FUNCTION_ARGS);

/*
 * Pattern matching functions
 */
extern Datum regex_match(PG_FUNCTION_ARGS);    /* Test if pattern matches */
extern Datum regex_search(PG_FUNCTION_ARGS);   /* Find first match */

/*
 * Utility functions
 */
extern RegEx *regex_create(const char *pattern, const char *options);
extern bool regex_is_valid(const char *pattern);
extern char *regex_get_pattern(RegEx *regex);
extern char *regex_get_options(RegEx *regex);
extern bool regex_has_option(RegEx *regex, char option);
extern void regex_set_options(RegEx *regex, const char *options);
extern RegEx *regex_clone(RegEx *regex);
extern void regex_free(RegEx *regex);
extern int regex_compare(RegEx *a, RegEx *b);
extern uint32 regex_hash(RegEx *regex);

/*
 * Options validation and normalization
 */
extern bool regex_options_valid(const char *options);
extern char *regex_normalize_options(const char *options);

/*
 * MongoDB regex option constants
 */
#define REGEX_OPTION_IGNORE_CASE    'i'   /* Case insensitive matching */
#define REGEX_OPTION_MULTILINE      'm'   /* ^ and $ match line boundaries */
#define REGEX_OPTION_DOTALL         's'   /* . matches newline characters */
#define REGEX_OPTION_EXTENDED       'x'   /* Extended regex syntax */

#endif							/* BSON_REGEX_H */
