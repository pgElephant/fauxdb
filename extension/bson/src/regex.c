/*
 * regex.c
 *
 * PostgreSQL BSON extension - RegEx type support
 *
 * This module implements RegEx type support for the BSON extension.
 * Provides MongoDB-compatible regular expression functionality.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    contrib/bson/regex.c
 */

#include "postgres.h"

#include <string.h>
#include <stdlib.h>
#include <regex.h>

#include "fmgr.h"
#include "utils/builtins.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"

#include "../include/regex.h"


/*
 * Input function for RegEx type
 * Format: /pattern/options or pattern,options
 */
PG_FUNCTION_INFO_V1(regex_in);

Datum
regex_in(PG_FUNCTION_ARGS)
{
	char	   *input = PG_GETARG_CSTRING(0);
	RegEx	   *regex;
	char	   *pattern;
	char	   *options = "";
	char	   *input_copy;
	char	   *comma_pos;
	
	if (!input || strlen(input) == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for RegEx")));

	input_copy = pstrdup(input);
	
	/* Check if format is /pattern/options */
	if (input_copy[0] == '/')
	{
		char *end_slash = strrchr(input_copy + 1, '/');
		if (end_slash && end_slash > input_copy + 1)
		{
			*end_slash = '\0';
			pattern = input_copy + 1;
			options = end_slash + 1;
		}
		else
		{
			/* No closing slash, treat as pattern only */
			pattern = input_copy + 1;
		}
	}
	/* Check if format is pattern,options */
	else if ((comma_pos = strchr(input_copy, ',')) != NULL)
	{
		*comma_pos = '\0';
		pattern = input_copy;
		options = comma_pos + 1;
	}
	else
	{
		/* Plain pattern */
		pattern = input_copy;
	}

	regex = regex_create(pattern, options);
	
	/* Validate the pattern */
	if (!regex_is_valid(pattern))
	{
		pfree(input_copy);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_REGULAR_EXPRESSION),
				 errmsg("invalid regular expression pattern: \"%s\"", pattern)));
	}

	pfree(input_copy);
	PG_RETURN_POINTER(regex);
}

/*
 * Output function for RegEx type
 */
PG_FUNCTION_INFO_V1(regex_out);

Datum
regex_out(PG_FUNCTION_ARGS)
{
	RegEx	   *regex = (RegEx *) PG_GETARG_POINTER(0);
	StringInfoData buf;
	
	initStringInfo(&buf);
	
	/* Format as /pattern/options */
	appendStringInfoChar(&buf, '/');
	appendStringInfoString(&buf, regex->pattern);
	appendStringInfoChar(&buf, '/');
	if (regex->options && strlen(regex->options) > 0)
		appendStringInfoString(&buf, regex->options);

	PG_RETURN_CSTRING(buf.data);
}

/*
 * Binary input function for RegEx type
 */
PG_FUNCTION_INFO_V1(regex_recv);

Datum
regex_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	RegEx	   *regex;
	const char *pattern;
	const char *options;
	
	pattern = pq_getmsgstring(buf);
	options = pq_getmsgstring(buf);
	
	regex = regex_create(pattern, options);
	
	PG_RETURN_POINTER(regex);
}

/*
 * Binary output function for RegEx type
 */
PG_FUNCTION_INFO_V1(regex_send);

Datum
regex_send(PG_FUNCTION_ARGS)
{
	RegEx	   *regex = (RegEx *) PG_GETARG_POINTER(0);
	StringInfoData buf;
	
	pq_begintypsend(&buf);
	pq_sendstring(&buf, regex->pattern);
	pq_sendstring(&buf, regex->options);
	
	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

/*
 * Equality operator for RegEx type
 */
PG_FUNCTION_INFO_V1(regex_eq);

Datum
regex_eq(PG_FUNCTION_ARGS)
{
	RegEx	   *a = (RegEx *) PG_GETARG_POINTER(0);
	RegEx	   *b = (RegEx *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(strcmp(a->pattern, b->pattern) == 0 && 
				   strcmp(a->options, b->options) == 0);
}

/*
 * Inequality operator for RegEx type
 */
PG_FUNCTION_INFO_V1(regex_ne);

Datum
regex_ne(PG_FUNCTION_ARGS)
{
	RegEx	   *a = (RegEx *) PG_GETARG_POINTER(0);
	RegEx	   *b = (RegEx *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(strcmp(a->pattern, b->pattern) != 0 || 
				   strcmp(a->options, b->options) != 0);
}

/*
 * Match function for RegEx type - test if pattern matches input string
 */
PG_FUNCTION_INFO_V1(regex_match);

Datum
regex_match(PG_FUNCTION_ARGS)
{
	RegEx	   *regex = (RegEx *) PG_GETARG_POINTER(0);
	text	   *input = PG_GETARG_TEXT_P(1);
	char	   *input_str;
	regex_t		compiled_regex;
	int			regex_flags = 0;
	int			result;
	bool		matches = false;
	char		error_buf[256];

	input_str = text_to_cstring(input);

	/* Set regex flags based on options */
	if (regex_has_option(regex, 'i'))
		regex_flags |= REG_ICASE;	/* Case insensitive */
	if (regex_has_option(regex, 'm'))
		regex_flags |= REG_NEWLINE;	/* Multiline */
	if (regex_has_option(regex, 'x'))
		regex_flags |= REG_EXTENDED;	/* Extended syntax */

	/* Always use extended regex */
	regex_flags |= REG_EXTENDED;

	/* Compile the regex */
	result = regcomp(&compiled_regex, regex->pattern, regex_flags);
	if (result != 0)
	{
		char error_buf[256];
		regerror(result, &compiled_regex, error_buf, sizeof(error_buf));
		pfree(input_str);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_REGULAR_EXPRESSION),
				 errmsg("invalid regular expression: %s", error_buf)));
	}

	/* Execute the regex */
	result = regexec(&compiled_regex, input_str, 0, NULL, 0);
	matches = (result == 0);

	/* Clean up */
	regfree(&compiled_regex);
	pfree(input_str);

	PG_RETURN_BOOL(matches);
}

/*
 * Search function for RegEx type - find matches in input string
 */
PG_FUNCTION_INFO_V1(regex_search);

Datum
regex_search(PG_FUNCTION_ARGS)
{
	RegEx	   *regex = (RegEx *) PG_GETARG_POINTER(0);
	text	   *input = PG_GETARG_TEXT_P(1);
	char	   *input_str;
	regex_t		compiled_regex;
	regmatch_t	match;
	int			regex_flags = 0;
	int			result;
	text	   *match_result = NULL;

	input_str = text_to_cstring(input);

	/* Set regex flags based on options */
	if (regex_has_option(regex, 'i'))
		regex_flags |= REG_ICASE;
	if (regex_has_option(regex, 'm'))
		regex_flags |= REG_NEWLINE;
	if (regex_has_option(regex, 'x'))
		regex_flags |= REG_EXTENDED;

	regex_flags |= REG_EXTENDED;

	/* Compile the regex */
	result = regcomp(&compiled_regex, regex->pattern, regex_flags);
	if (result != 0)
	{
		char error_buf[256];
		regerror(result, &compiled_regex, error_buf, sizeof(error_buf));
		pfree(input_str);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_REGULAR_EXPRESSION),
				 errmsg("invalid regular expression: %s", error_buf)));
	}

	/* Execute the regex */
	result = regexec(&compiled_regex, input_str, 1, &match, 0);
	if (result == 0 && match.rm_so >= 0)
	{
		/* Extract the matched substring */
		int match_len = match.rm_eo - match.rm_so;
		char *match_str = palloc(match_len + 1);
		memcpy(match_str, input_str + match.rm_so, match_len);
		match_str[match_len] = '\0';
		match_result = cstring_to_text(match_str);
		pfree(match_str);
	}

	/* Clean up */
	regfree(&compiled_regex);
	pfree(input_str);

	if (match_result)
		PG_RETURN_TEXT_P(match_result);
	else
		PG_RETURN_NULL();
}

/*
 * Create RegEx structure
 */
RegEx *
regex_create(const char *pattern, const char *options)
{
	RegEx	   *regex = (RegEx *) palloc(sizeof(RegEx));

	regex->pattern = pstrdup(pattern);
	regex->options = pstrdup(options ? options : "");

	return regex;
}

/*
 * Check if RegEx pattern is valid
 */
bool
regex_is_valid(const char *pattern)
{
	regex_t compiled_regex;
	int result;
	
	if (!pattern || strlen(pattern) == 0)
		return false;
		
	/* Try to compile the pattern to check validity */
	result = regcomp(&compiled_regex, pattern, REG_EXTENDED | REG_NOSUB);
	if (result == 0)
	{
		regfree(&compiled_regex);
		return true;
	}
	
	return false;
}

/*
 * Get RegEx pattern
 */
char *
regex_get_pattern(RegEx *regex)
{
	return regex ? regex->pattern : NULL;
}

/*
 * Get RegEx options
 */
char *
regex_get_options(RegEx *regex)
{
	return regex ? regex->options : NULL;
}

/*
 * Check if RegEx has specific option
 */
bool
regex_has_option(RegEx *regex, char option)
{
	return regex && regex->options && strchr(regex->options, option) != NULL;
}

/*
 * Set RegEx options
 */
void
regex_set_options(RegEx *regex, const char *options)
{
	if (!regex)
		return;
		
	if (regex->options)
		pfree(regex->options);
		
	regex->options = pstrdup(options ? options : "");
}

/*
 * Clone a RegEx
 */
RegEx *
regex_clone(RegEx *regex)
{
	if (!regex)
		return NULL;
		
	return regex_create(regex->pattern, regex->options);
}

/*
 * Free a RegEx structure
 */
void
regex_free(RegEx *regex)
{
	if (!regex)
		return;
		
	if (regex->pattern)
		pfree(regex->pattern);
	if (regex->options)
		pfree(regex->options);
	pfree(regex);
}

/*
 * Compare two RegEx structures
 */
int
regex_compare(RegEx *a, RegEx *b)
{
	int pattern_cmp;
	
	if (!a && !b)
		return 0;
	if (!a)
		return -1;
	if (!b)
		return 1;
		
	pattern_cmp = strcmp(a->pattern, b->pattern);
	if (pattern_cmp != 0)
		return pattern_cmp;
		
	return strcmp(a->options, b->options);
}

/*
 * Hash function for RegEx
 */
uint32
regex_hash(RegEx *regex)
{
	uint32 hash = 0;
	
	if (!regex)
		return 0;
		
	/* Simple hash combining pattern and options */
	if (regex->pattern)
	{
		char *p = regex->pattern;
		while (*p)
		{
			hash = ((hash << 5) + hash) + (unsigned char) *p;
			p++;
		}
	}
	
	if (regex->options)
	{
		char *p = regex->options;
		while (*p)
		{
			hash = ((hash << 5) + hash) + (unsigned char) *p;
			p++;
		}
	}
	
	return hash;
}

/*
 * Check if options string contains only valid regex options
 */
bool
regex_options_valid(const char *options)
{
	const char *valid_options = "imsx";
	
	if (!options)
		return true;
		
	/* Valid MongoDB regex options: i, m, s, x */
	
	while (*options)
	{
		if (!strchr(valid_options, *options))
			return false;
		options++;
	}
	
	return true;
}

/*
 * Normalize options string (remove duplicates, sort)
 */
char *
regex_normalize_options(const char *options)
{
	bool has_i = false, has_m = false, has_s = false, has_x = false;
	StringInfoData buf;
	
	if (!options)
		return pstrdup("");
		
	/* Check which options are present */
	while (*options)
	{
		switch (*options)
		{
			case 'i': has_i = true; break;
			case 'm': has_m = true; break;
			case 's': has_s = true; break;
			case 'x': has_x = true; break;
		}
		options++;
	}
	
	/* Build normalized string */
	initStringInfo(&buf);
	if (has_i) appendStringInfoChar(&buf, 'i');
	if (has_m) appendStringInfoChar(&buf, 'm');
	if (has_s) appendStringInfoChar(&buf, 's');
	if (has_x) appendStringInfoChar(&buf, 'x');
	
	return buf.data;
}
