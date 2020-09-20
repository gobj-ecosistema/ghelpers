/***********************************************************************
 *              JSON_HELPER.C
 *              Helpers for the great jansson library.
 *              Copyright (c) 2013, 2015 Niyamaka.
 *              All Rights Reserved.
 ***********************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "13_json_helper.h"

/***************************************************************************
 *  Convert any json string to json binary.
 ***************************************************************************/
PUBLIC json_t * nonlegalstring2json(const char *str, BOOL verbose)
{
    if(empty_string(str)) {
        return 0;
    }
    size_t flags = JSON_DECODE_ANY;
    json_error_t error;
    json_t *jn = json_loads(str, flags, &error);
    if(!jn) {
        if(verbose) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "json_loads(1) FAILED",
                "str",          "%s", str,
                "error",        "%s", error.text,
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                NULL
            );
            log_debug_dump(0, str, strlen(str), "json_loads(1) FAILED");
        }
    }
    return jn;
}

/***************************************************************************
 *  Convert any json string to json binary.
 ***************************************************************************/
PUBLIC json_t * nonlegalbuffer2json(const char *bf, size_t len, BOOL verbose)
{
    size_t flags = JSON_DECODE_ANY;
    json_error_t error;

    json_t *jn = json_loadb(bf, len, flags, &error);
    if(!jn) {
        if(verbose) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "json_loads(2) FAILED",
                "str",          "%s", bf,
                "error",        "%s", error.text,
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                NULL
            );
            log_debug_dump(0, bf, len, "json_loads(2) FAILED");
        }
    }
    return jn;
}

/***************************************************************************
 *  Convert a legal json string to json binary.
 *  legal json string: MUST BE an array [] or object {}
 ***************************************************************************/
PUBLIC json_t * legalstring2json(const char* str, BOOL verbose)
{
    size_t flags = 0;
    json_error_t error;

    json_t *jn = json_loads(str, flags, &error);
    if(!jn) {
        if(verbose) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "json_loads(3) FAILED",
                "str",          "%s", str,
                "error",        "%s", error.text,
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                NULL
            );
        }
    }
    return jn;
}

/***************************************************************************
   Formatted output conversion to string json
   Maximum output size of 4K
 ***************************************************************************/
PUBLIC json_t *json_local_sprintf(const char *fmt, ...)
{
    va_list ap;
    char temp[4*1024];

    va_start(ap, fmt);
    vsnprintf(temp, sizeof(temp), fmt, ap);
    va_end(ap);

    return json_string(temp);
}

/***************************************************************************
 *  Print json to stdout
 ***************************************************************************/
PUBLIC int print_json(json_t *jn)
{
    if(!jn || jn->refcount <= 0) {
        fprintf(stdout, "ERROR print_json(1): json NULL or refcount is 0\n");
        return -1;
    }
    size_t flags = JSON_INDENT(4)|JSON_ENCODE_ANY;
    fprintf(stdout, "refcount: %d\n", (int)jn->refcount);
    json_dumpf(jn, stdout, flags);
    fprintf(stdout, "\n");
    return 0;
}

/***************************************************************************
 *  Print json to stdout
 ***************************************************************************/
PUBLIC int print_json2(const char *label, json_t *jn)
{
    if(!jn || jn->refcount <= 0) {
        fprintf(stdout, "ERROR print_json(2): json NULL or refcount is 0\n");
        return -1;
    }
    size_t flags = JSON_INDENT(4); //JSON_SORT_KEYS |
    fprintf(stdout, "%s ==>\n", label);
    json_dumpf(jn, stdout, flags);
    fprintf(stdout, "\n");
    return 0;
}

/***************************************************************************
    Return json type name
 ***************************************************************************/
PUBLIC const char *json_type_name(json_t *jn)
{
    if(!jn) {
        return "???";
    }
    switch(json_typeof(jn)) {
    case JSON_OBJECT:
        return "object";
    case JSON_ARRAY:
        return "array";
    case JSON_STRING:
        return "string";
    case JSON_INTEGER:
        return "integer";
    case JSON_REAL:
        return "real";
    case JSON_TRUE:
        return "true";
    case JSON_FALSE:
        return "false";
    case JSON_NULL:
        return "null";
    default:
        return "???";
    }
}

/****************************************************************************
 *  Indent, return spaces multiple of depth level gobj.
 *  With this, we can see the trace messages indenting according
 *  to depth level.
 ****************************************************************************/
PRIVATE char *_tab(char *bf, int bflen, int deep)
{
    int i;

    for(i=0; i<deep*4 && i<bflen-1; i++) {
        bf[i] = ' ';
    }
    bf[i] = '\0';
    return bf;
}

/****************************************************************************
 *  Trace machine function
 ****************************************************************************/
PRIVATE void _trace_json(int deep, const char *fmt, ...)
{
    va_list ap;
    char bf[2*1024];
    _tab(bf, sizeof(bf), deep);

    va_start(ap, fmt);
    int len = strlen(bf);
    vsnprintf(bf+len, sizeof(bf)-len, fmt, ap);
    va_end(ap);

    fprintf(stdout, "%s", bf);
}

/***************************************************************************
 *  Print json with refcounts
 ***************************************************************************/
PRIVATE int _debug_json(int deep, json_t *jn, BOOL inside_list, BOOL inside_dict)
{
    if(!jn) {
        fprintf(stdout, "ERROR _debug_json(): json NULL\n");
        return -1;
    }
    if(jn->refcount <= 0) {
        fprintf(stdout, "ERROR _debug_json(): refcount is 0\n");
        return -1;
    }
    if(json_is_array(jn)) {
        size_t idx;
        json_t *jn_value;
        _trace_json(inside_dict?0:deep, "[ (%d)\n", jn->refcount);
        deep++;
        json_array_foreach(jn, idx, jn_value) {
            if(idx > 0) {
                _trace_json(0, ",\n");
            }
            _debug_json(deep, jn_value, 1, 0);
        }
        deep--;
        _trace_json(0, "\n");
        _trace_json(deep, "]");
        if(!inside_dict) {
            _trace_json(deep, "\n");
        }

    } else if(json_is_object(jn)) {
        const char *key;
        json_t *jn_value;

        _trace_json(inside_dict?0:deep, "{ (%d)\n", jn->refcount);
        deep++;
        int idx = 0;
        int max_idx = json_object_size(jn);
        json_object_foreach(jn, key, jn_value) {
            _trace_json(deep, "\"%s\": ", key);
            _debug_json(deep, jn_value, 0, 1);
            idx++;
            if(idx < max_idx) {
                _trace_json(0, ",\n");
            } else {
               _trace_json(0, "\n");
            }
        }
        deep--;
        _trace_json(deep, "}");
    } else if(json_is_string(jn)) {
        _trace_json(inside_list?deep:0, "\"%s\" (%d)", json_string_value(jn), jn->refcount);
    } else if(json_is_integer(jn)) {
        _trace_json(inside_list?deep:0, "%"JSON_INTEGER_FORMAT" (%d)", json_integer_value(jn), jn->refcount);
    } else if(json_is_real(jn)) {
        _trace_json(inside_list?deep:0, "%.2f (%d)", json_real_value(jn), jn->refcount);
    } else if(json_is_true(jn))  {
        _trace_json(inside_list?deep:0, "true");
    } else if(json_is_false(jn)) {
        _trace_json(inside_list?deep:0, "false");
    } else if(json_is_null(jn)) {
        _trace_json(inside_list?deep:0, "null");
    } else {
    }

    return 0;
}

/***************************************************************************
 *  Print json with refcounts
 ***************************************************************************/
PUBLIC int debug_json(json_t *jn)
{
    if(!jn || jn->refcount <= 0) {
        fprintf(stdout, "ERROR print_json(3): json NULL or refcount is 0\n");
        return -1;
    }
    fprintf(stdout, "\n");
    _debug_json(0, jn, 0, 0);
    fprintf(stdout, "\n");
    fflush(stdout);
    return 0;
}

/***************************************************************************
 *  Check if a item are in `list` array:
 ***************************************************************************/
PUBLIC BOOL json_str_in_list(json_t *jn_list, const char *str, BOOL ignore_case)
{
    size_t idx;
    json_t *jn_str;

    if(!json_is_array(jn_list)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        return FALSE;
    }
    if(!str) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "str NULL",
            NULL
        );
        return FALSE;
    }

    json_array_foreach(jn_list, idx, jn_str) {
        if(!json_is_string(jn_str)) {
            continue;
        }
        const char *_str = json_string_value(jn_str);
        if(ignore_case) {
            if(strcasecmp(_str, str)==0)
                return TRUE;
        } else {
            if(strcmp(_str, str)==0)
                return TRUE;
        }
    }

    return FALSE;
}

/***************************************************************************
 *  Get a string value from an json list search by idx
 ***************************************************************************/
PUBLIC const char *json_list_str(json_t *jn_list, size_t idx)
{
    json_t *jn_str;
    const char *str;

    if(!json_is_array(jn_list)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        return 0;
    }

    jn_str = json_array_get(jn_list, idx);
    if(jn_str && !json_is_string(jn_str)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "value MUST BE a json string",
            NULL);
        return 0;
    }

    str = json_string_value(jn_str);
    return str;
}

/***************************************************************************
    Get a the idx of string value in a strings json list.
 ***************************************************************************/
PUBLIC size_t json_list_str_index(json_t *jn_list, const char *str, BOOL ignore_case)
{
    if(!json_is_array(jn_list)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        return 0;
    }

    size_t idx;
    json_t *jn_str;

    json_array_foreach(jn_list, idx, jn_str) {
        if(!json_is_string(jn_str)) {
            continue;
        }
        const char *_str = json_string_value(jn_str);
        if(ignore_case) {
            if(strcasecmp(_str, str)==0)
                return idx;
        } else {
            if(strcmp(_str, str)==0)
                return idx;
        }
    }

    return -1;
}

/***************************************************************************
    Get a integer value from an json list search by idx
 ***************************************************************************/
PUBLIC json_int_t json_list_int(json_t *jn_list, size_t idx)
{
    json_t *jn_int;
    json_int_t i;

    if(!json_is_array(jn_list)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        return 0;
    }

    jn_int = json_array_get(jn_list, idx);
    if(jn_int && !json_is_integer(jn_int)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "value MUST BE a json integer",
            NULL);
        return 0;
    }

    i = json_integer_value(jn_int);
    return i;
}

/***************************************************************************
    Get a the idx of integer value in a integers json list.
 ***************************************************************************/
PUBLIC size_t json_list_int_index(json_t *jn_list, json_int_t value)
{
    if(!json_is_array(jn_list)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        return 0;
    }

    size_t idx = 0;
    json_t *jn_value;
    json_array_foreach(jn_list, idx, jn_value) {
        if(json_is_integer(jn_value)) {
            json_int_t value_ = json_integer_value(jn_value);
            if(value_ == value) {
                return idx;
            }
        }
    }

    return -1;
}

/***************************************************************************
 *  Any json to string
 *  Remember gbmem_free the returned string
 ***************************************************************************/
PUBLIC char *json2str(const json_t *jn) // jn not owned
{
    if(!jn) {
        return 0;
    }
    size_t flags = JSON_ENCODE_ANY | JSON_INDENT(4) | JSON_REAL_PRECISION(get_real_precision());
    return json_dumps(jn, flags);
}

/***************************************************************************
 *  Any json to ugly (non-tabular) string
 *  Remember gbmem_free the returned string
 ***************************************************************************/
PUBLIC char *json2uglystr(const json_t *jn) // jn not owned
{
    if(!jn) {
        return 0;
    }
    size_t flags = JSON_ENCODE_ANY | JSON_COMPACT | JSON_REAL_PRECISION(get_real_precision());
    return json_dumps(jn, flags);
}

/***************************************************************************
 *  Simple json (str/int/real) to string
 *  Remember gbmem_free the returned string
 ***************************************************************************/
PUBLIC char *simplejson2str(const json_t *jn)
{
    if(!jn) {
        return 0;
    }
    char *s = 0;

    if(json_is_string(jn)) {
        s = gbmem_strdup(json_string_value(jn));
    } else if(json_is_integer(jn)) {
        json_int_t ji = json_integer_value(jn);
        char temp[64];
        snprintf(temp, sizeof(temp), "%" JSON_INTEGER_FORMAT, ji);
        s = gbmem_strdup(temp);
    } else if(json_is_real(jn)) {
        double jd = json_real_value(jn);
        char temp[64];
        snprintf(temp, sizeof(temp), "%f", jd);
        s = gbmem_strdup(temp);
    }
    return s;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC double json2number(const json_t *jn)
{
    if(!jn) {
        return 0;
    }
    double d;

    if(json_is_string(jn)) {
        const char *s = json_string_value(jn);
        d = atof(s);
    } else if(json_is_integer(jn)) {
        d = json_integer_value(jn);
    } else if(json_is_real(jn)) {
        d = json_real_value(jn);
    } else {
        d = 0;
    }
    return d;
}

/***************************************************************************
 *  Compare two json strings or numbers (integer is converted to string)
 ***************************************************************************/
PRIVATE int compare_by_string(const void *key1, const void *key2)
{
    const json_t *jn_key1 = *((const json_t **)key1);
    const json_t *jn_key2 = *((const json_t **)key2);

    char *key1_str = simplejson2str(jn_key1);
    char *key2_str = simplejson2str(jn_key2);
    int ret = strcmp(key1_str, key2_str);
    gbmem_free(key1_str);
    gbmem_free(key2_str);
    return ret;
}

/***************************************************************************
 *  Compare two json strings or numbers (integer is converted to string)
 ***************************************************************************/
PRIVATE int compare_by_number(const void *key1, const void *key2)
{
    const json_t *jn_key1 = *((const json_t **)key1);
    const json_t *jn_key2 = *((const json_t **)key2);

    double key1_number = json2number(jn_key1);
    double key2_number = json2number(jn_key2);
    if(key1_number > key2_number) {
        return 1;
    } else if(key1_number < key2_number) {
        return -1;
    } else {
        return 0;
    }
}

/***************************************************************************
 *      Sort a str/int list by string
 ***************************************************************************/
PRIVATE json_t *_sort_json_list(
    json_t *jn_list,
    int (*cmp_func)(const void *, const void *))
{
    json_t **keys;
    size_t size;

    size = json_array_size(jn_list);
    keys = gbmem_malloc(size * sizeof(json_t *));
    if(!keys) {
        JSON_DECREF(jn_list);
        return 0;
    }

    size_t idx;
    json_t *jn_value;
    json_array_foreach(jn_list, idx, jn_value) {
        *(keys + idx) = jn_value;
    }
    qsort(keys, size, sizeof(json_t *), cmp_func);

    json_t *jn_sorted_list = json_array();
    for(int i=0; i<size; i++) {
        json_array_append(jn_sorted_list, *(keys + i));
    }

    JSON_DECREF(jn_list);
    gbmem_free(keys);
    return jn_sorted_list;
}

/***************************************************************************
 *      Sort a str/int list by number
 ***************************************************************************/
PUBLIC json_t *sort_json_list_by_number(json_t *jn_list)
{
    return _sort_json_list(jn_list, compare_by_number);
}

/***************************************************************************
 *      Sort a str/int list by string
 ***************************************************************************/
PUBLIC json_t *sort_json_list_by_string(json_t *jn_list)
{
    return _sort_json_list(jn_list, compare_by_string);
}

/***************************************************************************
 *  Find a json value in the list.
 *  Return idx or -1 if not found or the idx relative to 0.
 ***************************************************************************/
PRIVATE int json_list_find(json_t *list, json_t *value)
{
    size_t idx_found = -1;
    size_t flags = JSON_COMPACT|JSON_ENCODE_ANY; //|JSON_SORT_KEYS;
    size_t idx;
    json_t *_value;
    char *s_new_value = json_dumps(value, flags);
    if(s_new_value) {
        json_array_foreach(list, idx, _value) {
            char *s_value = json_dumps(_value, flags);
            if(s_value) {
                if(strcmp(s_value, s_new_value)==0) {
                    idx_found = idx;
                    jsonp_free(s_value);
                    break;
                } else {
                    jsonp_free(s_value);
                }
            }
        }
        jsonp_free(s_new_value);
    }
    return idx_found;
}

/***************************************************************************
 *  Check if a list is a integer range:
 *      - must be a list of two integers (first < second)
 ***************************************************************************/
PRIVATE BOOL json_is_range(json_t *list)
{
    if(json_array_size(list) != 2)
        return FALSE;

    json_int_t first = json_integer_value(json_array_get(list, 0));
    json_int_t second = json_integer_value(json_array_get(list, 1));
    if(first <= second)
        return TRUE;
    else
        return FALSE;
}

/***************************************************************************
 *  Return a expanded integer range
 ***************************************************************************/
PRIVATE json_t *json_range_list(json_t *list)
{
    if(!json_is_range(list))
        return 0;
    json_int_t first = json_integer_value(json_array_get(list, 0));
    json_int_t second = json_integer_value(json_array_get(list, 1));
    json_t *range = json_array();
    for(int i=first; i<=second; i++) {
        json_t *jn_int = json_integer(i);
        json_array_append_new(range, jn_int);
    }
    return range;

}

/***************************************************************************
 *  Extend array values.
 *  If as_set is TRUE then not repeated values
 ***************************************************************************/
PRIVATE int json_list_update(json_t *list, json_t *other, BOOL as_set)
{
    if(!json_is_array(list) || !json_is_array(other)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parameters must be lists",
            NULL
        );
        return -1;
    }
    size_t idx;
    json_t *value;
    json_array_foreach(other, idx, value) {
        if(as_set) {
            int idx = json_list_find(list, value);
            if(idx < 0) {
                json_array_append(list, value);
            }
        } else {
            json_array_append(list, value);
        }
    }
    return 0;
}

/***************************************************************************
 *  Build a list (set) with lists of integer ranges.
 *  [[#from, #to], [#from, #to], #integer, #integer, ...] -> list
 *  WARNING: Arrays of two integers are considered range of integers.
 *  Arrays of one or more of two integers are considered individual integers.
 *
 *  Return the json list
 ***************************************************************************/
PUBLIC json_t *json_listsrange2set(json_t *listsrange) // not owned
{
    if(!json_is_array(listsrange)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parameters must be lists",
            NULL
        );
        return 0;
    }
    json_t *ln_list = json_array();

    size_t idx;
    json_t *value;
    json_array_foreach(listsrange, idx, value) {
        if(json_is_integer(value)) {
            // add new integer item
            if(json_list_find(ln_list, value)<0) {
                json_array_append(ln_list, value);
            }
        } else if(json_is_array(value)) {
            // add new integer list or integer range
            if(json_is_range(value)) {
                json_t *range = json_range_list(value);
                if(range) {
                    json_list_update(ln_list, range, TRUE);
                    JSON_DECREF(range);
                }
            } else {
                json_list_update(ln_list, value, TRUE);
            }
        } else {
            // ignore the rest
            continue;
        }
    }

    return ln_list;
}

/***************************************************************************
 *  Convert a integer or a list of integer or a list of list of integer
 *  in a set of integers
 ***************************************************************************/
PUBLIC json_t *json_expand_integer_list(json_t *jn_id)  // not owned
{
    json_t *jn_ids;

    if(json_is_array(jn_id)) {
        jn_ids = json_listsrange2set(jn_id);
    } else if(json_is_integer(jn_id)) {
        jn_ids = json_array();
        json_int_t i = json_integer_value(jn_id);
        json_array_append_new(jn_ids, json_integer(i));
    } else {
        jn_ids = json_array();
    }
    return jn_ids;
}

/***************************************************************************
 *  Convert a json integer list in a c int **
 *  Remember free the returned value with gbmem_free().
 *  Fields of array that are not integers or be 0 they are set with -1
    The last item is cero (your stop point).
 ***************************************************************************/
PUBLIC json_int_t *jsonlist2c(json_t *jn_int_list)
{
    if(!jn_int_list) {
        return 0;
    }
    int n = json_array_size(jn_int_list);
    json_int_t *c_int_list = gbmem_malloc((n * sizeof(json_int_t)) + 1);

    json_int_t *pint = c_int_list;
    size_t idx;
    json_t *jn_int;
    json_array_foreach(jn_int_list, idx, jn_int) {
        json_int_t i = json_integer_value(jn_int);
        if(!i) {
            i = (json_int_t)-1;
        }
        *(pint) = i;
        pint++;
    }

    return c_int_list;
}

/***************************************************************************
 *  Check if id is in ids list
 ***************************************************************************/
PUBLIC BOOL int_in_list(json_int_t id, json_int_t *ids_list)
{
    if(!ids_list) {
        return FALSE;
    }
    while(*ids_list) {
        if(*ids_list == id) {
            return TRUE;
        }
        ids_list++;
    }
    return FALSE;
}

/***************************************************************************
 *  Check if id is in js list
 ***************************************************************************/
PUBLIC BOOL int_in_jn_list(json_int_t id, json_t *jn_list)
{
    size_t idx;
    json_t *jn_int;
    json_array_foreach(jn_list, idx, jn_int) {
        json_int_t i = json_integer_value(jn_int);
        if(i>0 && i==id) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Check if id is in jn list of dict
 ***************************************************************************/
PUBLIC BOOL int_in_dict_list(json_int_t id, json_t *jn_dict, const char *list_name)
{
    json_t *jn_list = json_object_get(jn_dict, list_name);
    if(jn_list) {
        return int_in_jn_list(id, jn_list);
    }
    return FALSE;
}

/***************************************************************************
 *  Split s in a json list of strings splitted by separator
 ***************************************************************************/
PUBLIC json_t *split_string(char *s, char separator)
{
    json_t *jn_list = json_array();

    char *p;
    while((p=strchr(s, separator))) {
        *p = 0;
        json_array_append_new(jn_list, json_string(s));
        *p = separator;
        s = p + 1;
    }
    if(!empty_string(s)) {
        json_array_append_new(jn_list, json_string(s));
    }
    return jn_list;
}

/***************************************************************************
    Extract the keys of dict in a C const char *[]
    WARNING remember free with free_str_list()
 ***************************************************************************/
PUBLIC const char **extract_keys(json_t *dict, int underscores)
{
    if(!dict) {
        return 0;
    }
    const char **strls = gbmem_malloc((json_object_size(dict)+1) * sizeof(char *));
    int idx = 0;

    const char *key;
    json_t *jn_value;
    json_object_foreach(dict, key, jn_value) {
        if(underscores) {
            int u;
            for(u=0; u<strlen(key); u++) {
                if(key[u] != '_') {
                    break;
                }
            }
            if(u == underscores) {
                continue;
            }
        }
        *(strls + idx) = gbmem_strdup(key);
        idx++;
    }

    return strls;
}

/***************************************************************************
    Free a list of char *.
    Both inside list and list are gbmem_malloc'ed
 ***************************************************************************/
PUBLIC void free_str_list(const char **strls)
{
    if(!strls) {
        return;
    }
    char **p = (char **)strls;
    while(*p) {
        gbmem_free(*p);
        p++;
    }
    gbmem_free(strls);
}

/***************************************************************************
 *  fields: DESC str array with: key, type, defaults
 *  type can be: str, int, real, bool, null, dict, list
 ***************************************************************************/
PUBLIC json_t *create_json_record(
    const json_desc_t *json_desc
)
{
    if(!json_desc) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "DESC null",
            NULL
        );
        return 0;
    }
    json_t *jn = json_object();

    while(json_desc->name) {
        if(empty_string(json_desc->name)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "DESC without key field",
                NULL
            );
            break;
        }
        if(empty_string(json_desc->type)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "DESC without type field",
                NULL
            );
            break;
        }
        const char *name = json_desc->name;
        const char *defaults = json_desc->defaults;

        SWITCHS(json_desc->type) {
            CASES("str")
            CASES("string")
                json_object_set_new(jn, name, json_string(defaults));
                break;
            CASES("int")
            CASES("integer")
                unsigned long v=0;
                if(*defaults == '0') {
                    v = strtoul(defaults, 0, 8);
                } else if(*defaults == 'x') {
                    v = strtoul(defaults, 0, 16);
                } else {
                    v = strtoul(defaults, 0, 10);
                }
                json_object_set_new(jn, name, json_integer(v));
                break;
            CASES("real")
                json_object_set_new(jn, name, json_real(atof(defaults)));
                break;
            CASES("bool")
                if(strcasecmp(defaults, "true")==0) {
                    json_object_set_new(jn, name, json_true());
                } else if(strcasecmp(defaults, "false")==0) {
                    json_object_set_new(jn, name, json_false());
                } else {
                    json_object_set_new(jn, name, atoi(defaults)?json_true():json_false());
                }
                break;
            CASES("null")
                json_object_set_new(jn, name, json_null());
                break;
            CASES("dict")
                char desc_name[80+1];
                if(sscanf(defaults, "{%80s}", desc_name)==1) {
                    //get_fields(db_tranger_desc, desc_name, TRUE); // only to test fields
                } else if(!empty_string(defaults)) {
                    //get_fields(db_tranger_desc, defaults, TRUE); // only to test fields
                }
                json_object_set_new(jn, name, json_object());
                break;
            CASES("list")
                char desc_name[80+1];
                if(sscanf(defaults, "{%80s}", desc_name)==1) {
                    //get_fields(db_tranger_desc, desc_name, TRUE); // only to test fields
                }
                json_object_set_new(jn, name, json_array());
                break;
            DEFAULTS
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "process",      "%s", get_process_name(),
                    "hostname",     "%s", get_host_name(),
                    "pid",          "%d", get_pid(),
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Type UNKNOWN",
                    "type",         "%s", json_desc->type,
                    NULL
                );
                break;
        } SWITCHS_END;

        json_desc++;
    }

    return jn;
}

/***************************************************************************
 *  If exclusive then let file opened and return the fd, else close the file
 ***************************************************************************/
PUBLIC json_t *load_persistent_json(
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error,
    int *pfd,
    BOOL exclusive
)
{
    if(pfd) {
        *pfd = -1;
    }

    /*
     *  Full path
     */
    char full_path[PATH_MAX];
    build_path2(full_path, sizeof(full_path), directory, filename);

    if(access(full_path, 0)!=0) {
        log_critical(on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot load json, file not exist.",
            "path",         "%s", full_path,
            NULL
        );
        return 0;
    }

    int fd;
    if(exclusive) {
        fd = open_exclusive(full_path, O_RDONLY|O_NOFOLLOW, 0);
        fcntl(fd, F_SETFD, FD_CLOEXEC); // Que no vaya a los child
    } else {
        fd = open(full_path, O_RDONLY|O_NOFOLLOW);
    }
    if(fd<0) {
        log_critical(on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open json file",
            "path",         "%s", full_path,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return 0;
    }

    json_t *jn = json_loadfd(fd, 0, 0);
    if(!jn) {
        log_critical(on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot load json file, bad json",
            NULL
        );
        close(fd);
        return 0;
    }
    if(!exclusive) {
        close(fd);
    } else {
        if(pfd) {
            *pfd = fd;
        }
    }
    return jn;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *load_json_from_file(
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error
)
{
    /*
     *  Full path
     */
    char full_path[PATH_MAX];
    build_path2(full_path, sizeof(full_path), directory, filename);

    if(access(full_path, 0)!=0) {
        return 0;
    }

    int fd = open(full_path, O_RDONLY|O_NOFOLLOW);
    if(fd<0) {
        log_critical(on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open json file",
            "path",         "%s", full_path,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return 0;
    }

    json_t *jn = json_loadfd(fd, 0, 0);
    if(!jn) {
        log_critical(on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot load json file, bad json",
            NULL
        );
    }
    close(fd);
    return jn;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int save_json_to_file(
    const char *directory,
    const char *filename,
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,        // Create file if not exists or overwrite.
    BOOL only_read,
    json_t *jn_data     // owned
)
{
    /*-----------------------------------*
     *  Create directory if not exists
     *-----------------------------------*/
    if(!is_directory(directory)) {
        if(!create) {
            JSON_DECREF(jn_data);
            return -1;
        }
        if(mkrdir(directory, 0, xpermission)<0) {
            log_critical(on_critical_error,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create directory",
                "directory",    "%s", directory,
                "errno",        "%s", strerror(errno),
                NULL
            );
            JSON_DECREF(jn_data);
            return -1;
        }
    }

    /*
     *  Full path
     */
    char full_path[PATH_MAX];
    if(empty_string(filename)) {
        snprintf(full_path, sizeof(full_path), "%s", directory);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, filename);
    }

    /*
     *  Create file
     */
    int fp = newfile(full_path, rpermission, create);
    if(fp < 0) {
        log_critical(on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot create json file",
            "filename",     "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn_data);
        return -1;
    }

    if(json_dumpfd(jn_data, fp, JSON_INDENT(4))<0) {
        log_critical(on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot write in json file",
            "errno",        "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn_data);
        return -1;
    }
    close(fp);
    if(only_read) {
        chmod(full_path, 0440);
    }
    JSON_DECREF(jn_data);

    return 0;
}












#ifdef PEPE

/***************************************************************************
 *  Check if all strings items in `strlist` array are in `list` array
 ***************************************************************************/
PUBLIC BOOL json_all_strlist_in_list(json_t *list, json_t *strlist, BOOL ignore_case)
{
    size_t idx;
    json_t *jn_str;

    json_array_foreach(strlist, idx, jn_str) {
        if(!json_is_string(jn_str)) {
            continue;
        }
        const char *str = json_string_value(jn_str);
        if(json_str_in_list(list, str, ignore_case))
            return TRUE;
    }

    return FALSE;
}


/***************************************************************************
 *  remove leading `c` chars of list'strings
 *  Return a new json array
 ***************************************************************************/
PUBLIC json_t *json_list_lstrip(json_t *list, char c)
{
    json_t *new_list = json_array();

    size_t idx;
    json_t *jn_str;

    json_array_foreach(list, idx, jn_str) {
        if(!json_is_string(jn_str)) {
            continue;
        }
        const char *str = json_string_value(jn_str);
        const char *s = lstrip(str, strlen(str), c);
        json_array_append_new(new_list, json_string(s));
    }
    return new_list;
}


/***************************************************************************
 *  like json_array_append but with a path into a dict
 ***************************************************************************/
PUBLIC int helper_json_array_append(json_t *kw, const char *path, json_t *value)
{
    if(!json_is_object(kw)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a dict",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }

    char *p = strchr(path, '.');
    if(p) {
        char segment[NAME_MAX];
        snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path);
        json_t *deep_dict = json_object_get(kw, segment);
        if(json_is_object(deep_dict)) {
            return helper_json_array_append(deep_dict, p+1, value);
        }
    }
    json_t *jn_array = json_object_get(kw, path);
    if(!jn_array) {
        /*
         *  Create array kw if not exists
         */
        jn_array = json_array();
        json_object_set_new(kw, path, jn_array);
    }
    if(!json_is_array(jn_array)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }

    int ret;
    if(json_is_array(value)) {
        ret = json_array_extend(jn_array, value);
    } else {
        ret = json_array_append(jn_array, value);
    }
    return ret;
}

/***************************************************************************
 *  like json_array_append_new but with a path into a dict
 ***************************************************************************/
PUBLIC int helper_json_array_append_new(json_t *kw, const char *path, json_t *value)
{
    if(!json_is_object(kw)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a dict",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }

    char *p = strchr(path, '.');
    if(p) {
        char segment[NAME_MAX];
        snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path);
        json_t *deep_dict = json_object_get(kw, segment);
        if(json_is_object(deep_dict)) {
            return helper_json_array_append_new(deep_dict, p+1, value);
        }
    }
    json_t *jn_array = json_object_get(kw, path);
    if(!jn_array) {
        /*
         *  Create array kw if not exists
         */
        jn_array = json_array();
        json_object_set_new(kw, path, jn_array);
    }
    if(!json_is_array(jn_array)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }

    int ret;
    if(json_is_array(value)) {
        ret = json_array_extend(jn_array, value);
        JSON_DECREF(value);
    } else {
        ret = json_array_append_new(jn_array, value);
    }
    return ret;
}

/***************************************************************************
 *      Json str's list -> to -> char **list
 *      WARNING remember gbmem_free the returned value
 *      jn_list is owned
 ***************************************************************************/
PRIVATE char **jn_str_list_to_c_str_list(json_t *jn_list, int *strlist_size)
{
    char **keys;
    size_t size;

    *strlist_size = 0;
    size = json_array_size(jn_list);
    keys = gbmem_malloc((size+1) * sizeof(char *));
    if(!keys) {
        json_decref(jn_list);
        return 0;
    }

    size_t idx;
    json_t *jn_value;
    json_array_foreach(jn_list, idx, jn_value) {
        const char *str = json_string_value(jn_value);
        *(keys + idx) = (char *)str;
    }
    *strlist_size = size;

    json_decref(jn_list);
    return keys;
}

#endif
