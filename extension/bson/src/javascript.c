#include "../include/javascript.h"
#include <string.h>
#include <stdlib.h>

PG_FUNCTION_INFO_V1(javascript_in);
Datum javascript_in(PG_FUNCTION_ARGS) {
    char *code = PG_GETARG_CSTRING(0);
    JavaScript *js = (JavaScript *) palloc(sizeof(JavaScript));
    js->code = pstrdup(code);
    PG_RETURN_POINTER(js);
}

PG_FUNCTION_INFO_V1(javascript_out);
Datum javascript_out(PG_FUNCTION_ARGS) {
    JavaScript *js = (JavaScript *) PG_GETARG_POINTER(0);
    char *str = (char *) palloc(strlen(js->code) + 1);
    strcpy(str, js->code);
    PG_RETURN_CSTRING(str);
}

PG_FUNCTION_INFO_V1(javascript_scope_in);
Datum javascript_scope_in(PG_FUNCTION_ARGS) {
    char *input = PG_GETARG_CSTRING(0);
    JavaScriptScope *js = (JavaScriptScope *) palloc(sizeof(JavaScriptScope));
    
    // Parse format: "code|scope" for simplicity
    char *separator = strchr(input, '|');
    if (separator) {
        *separator = '\0';
        js->code = pstrdup(input);
        js->scope = pstrdup(separator + 1);
    } else {
        js->code = pstrdup(input);
        js->scope = pstrdup("{}");
    }
    PG_RETURN_POINTER(js);
}

PG_FUNCTION_INFO_V1(javascript_scope_out);
Datum javascript_scope_out(PG_FUNCTION_ARGS) {
    JavaScriptScope *js = (JavaScriptScope *) PG_GETARG_POINTER(0);
    int len = strlen(js->code) + strlen(js->scope) + 2;
    char *str = (char *) palloc(len);
    snprintf(str, len, "%s|%s", js->code, js->scope);
    PG_RETURN_CSTRING(str);
}

JavaScript *javascript_create(const char *code) {
    JavaScript *js = (JavaScript *) malloc(sizeof(JavaScript));
    js->code = strdup(code);
    return js;
}

JavaScriptScope *javascript_scope_create(const char *code, const char *scope) {
    JavaScriptScope *js = (JavaScriptScope *) malloc(sizeof(JavaScriptScope));
    js->code = strdup(code);
    js->scope = strdup(scope ? scope : "{}");
    return js;
}

char *javascript_get_code(JavaScript *js) {
    return js->code;
}

char *javascript_scope_get_code(JavaScriptScope *js) {
    return js->code;
}

char *javascript_scope_get_scope(JavaScriptScope *js) {
    return js->scope;
}

bool javascript_is_valid(const char *code) {
    return code && strlen(code) > 0;
}
