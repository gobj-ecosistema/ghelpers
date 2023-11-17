/***********************************************************************
 *              JSON_HELPER.H
 *              Helpers for the great jansson library.
 *              Copyright (c) 2013, 2015 Niyamaka.
 *              All Rights Reserved.
 *              Date functions
 ***********************************************************************/
#pragma once

#include <stdarg.h>
#include <jansson.h>

/*
 *  Dependencies
 */
#include "03_json_config.h"
#include "11_gbmem.h"

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Macros
 *********************************************************************/
#define JSON_DECREF(json)                                           \
{                                                                   \
    if(json) {                                                      \
        if((json)->refcount <= 0) {                                 \
            log_error(LOG_OPT_TRACE_STACK,                          \
                "gobj",         "%s", __FILE__,                     \
                "function",     "%s", __FUNCTION__,                 \
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,        \
                "msg",          "%s", "BAD json_decref()",          \
                NULL                                                \
            );                                                      \
        } else {                                                    \
            json_decref(json);                                      \
        }                                                           \
        (json) = 0;                                                 \
    }                                                               \
}
#define JSON_INCREF(json)                                           \
{                                                                   \
    if(json) {                                                      \
        if((json)->refcount <= 0) {                                 \
            log_error(LOG_OPT_TRACE_STACK,                          \
                "gobj",         "%s", __FILE__,                     \
                "function",     "%s", __FUNCTION__,                 \
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,        \
                "msg",          "%s", "BAD json_incref()",          \
                NULL                                                \
            );                                                      \
        } else {                                                    \
            json_incref(json);                                      \
        }                                                           \
    }                                                               \
}

#define JSON_TYPEOF(json, tipo) (json && json_typeof(json)==tipo)


#define json_array_backward(array, index, value) \
    for(index = json_array_size(array) - 1; \
        index >= 0 && (value = json_array_get(array, index)); \
        index--)

/***********************************************************************
 *      Prototypes
 ***********************************************************************/

/**rst**
   Convert any json string to json binary.
**rst**/
PUBLIC json_t * nonlegalstring2json(const char* str, BOOL verbose);

/**rst**
   Convert any json buffer to json binary.
**rst**/
PUBLIC json_t * nonlegalbuffer2json(const char* bf, size_t len, BOOL verbose);

/**rst**
   Convert any json file to json binary.
**rst**/
PUBLIC json_t * nonlegalfile2json(const char *path, BOOL verbose);

/**rst**
   Convert a legal json string to json binary.
   Legal json string: MUST BE an array [] or object {}
**rst**/
PUBLIC json_t * legalstring2json(const char* str, BOOL verbose);

// Shortcut
#define str2json legalstring2json

/**rst**
   Formatted output conversion to string json
   Maximum output size of 4K
**rst**/
PUBLIC json_t *json_local_sprintf(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)));

/**rst**
    Print json to stdout
**rst**/
PUBLIC int print_json(json_t *jn);

/**rst**
    Print json to stdout with a prefix
**rst**/
PUBLIC int print_json2(const char *label, json_t *jn);

/**rst**
    Return json type name
**rst**/
PUBLIC const char *json_type_name(json_t *jn);

/**rst**
    Print json with refcounts to stdout
**rst**/
PUBLIC int debug_json(json_t *jn);

/**rst**
    Check if a json is in `list` array:
**rst**/
PUBLIC BOOL json_in_list(json_t *jn_list, json_t *jn);

/**rst**
    Check if a string item are in `list` array:
**rst**/
PUBLIC BOOL json_str_in_list(json_t *jn_list, const char *str, BOOL ignore_case);

/**rst**
    Get a string value from an json list search by index
**rst**/
PUBLIC const char *json_list_str(json_t *jn_list, size_t idx);

/**rst**
    Get a the idx of string value in a strings json list.
    Return -1 if not exist
**rst**/
PUBLIC size_t json_list_str_index(json_t *jn_list, const char *str, BOOL ignore_case);

/**rst**
    Get a integer value from an integers json list search by idx
**rst**/
PUBLIC json_int_t json_list_int(json_t *jn_list, size_t idx);

/**rst**
    Get a the idx of integer value in a integers json list.
    Return -1 if not exist
**rst**/
PUBLIC size_t json_list_int_index(json_t *jn_list, json_int_t value);

/**rst**
    Sort a string/number list
**rst**/
PUBLIC json_t *sort_json_list(json_t *jn_list); /* jn_list owned */

/**rst**
    Any json to indented string
    Remember gbmem_free the returned string
**rst**/
PUBLIC char *json2str(const json_t *jn); // jn not owned

/**rst**
    Any json to ugly (non-tabular) string
    Remember gbmem_free the returned string
**rst**/
PUBLIC char *json2uglystr(const json_t *jn); // jn not owned

/**rst**
    Simple json (str/int/real) to string
    Remember gbmem_free the returned string
**rst**/
PUBLIC char *simplejson2str(const json_t *jn);
/**rst**
    Simple json (str/int/real) to number (real)
**rst**/
PUBLIC double json2number(const json_t *jn);

/**rst**
    Sort a str/int list by number
**rst**/
PUBLIC json_t *sort_json_list_by_number(json_t *jn_list); // jn_list owned

/**rst**
    Sort a str/int list by string
**rst**/
PUBLIC json_t *sort_json_list_by_string(json_t *jn_list); // jn_list owned

/**rst**
    Build a list (set) with lists of integer ranges.
    [[#from, #to], [#from, #to], #integer, #integer, ...] -> list
    WARNING: Arrays of two integers are considered range of integers.
    Arrays of one or more of two integers are considered individual integers.

    Return the json list
**rst**/
PUBLIC json_t *json_listsrange2set(json_t *listsrange); // not owned

/**rst**
    Convert a integer or a list of integer or a list of list of integer in a set of integers
**rst**/
PUBLIC json_t *json_expand_integer_list(json_t *jn_id);  // not owned

/**rst**
    Convert a json integer list in a c int **
    Remember free the returned value with gbmem_free().
    Fields of array that are not integers or be 0 they are set with -1
    The last item is cero (your stop point).
**rst**/
PUBLIC json_int_t *jsonlist2c(json_t *jn_int_list); // not owned

/**rst**
 *  Check if id is in ids list
**rst**/
PUBLIC BOOL int_in_list(json_int_t id, json_int_t *ids_list);

/**rst**
 *  Check if id is in jn list
**rst**/
PUBLIC BOOL int_in_jn_list(json_int_t id, json_t *jn_list);

/**rst**
 *  Check if id is in jn list of dict
**rst**/
PUBLIC BOOL int_in_dict_list(json_int_t id, json_t *jn_dict, const char *list_name);

/**rst**
    Split s in a json list of strings splitted by separator
**rst**/
PUBLIC json_t *split_string(char *s, char separator);

/**rst**
    Extract the keys of dict in a C const char *[]
    WARNING remember free with free_str_list()
**rst**/
PUBLIC const char **extract_keys(json_t *dict, int underscores);

/**rst**
    Free a list of char *.
    Both inside list and list are gbmem_malloc'ed
**rst**/
PUBLIC void free_str_list(const char **strls);

/***************************************************************************
 *
 *  type can be: str, string, int, integer, real, bool, boolean, null, dict, list
 *  Example:

static json_desc_t jn_xxx_desc[] = {
    // Name         Type        Default     Fillspace
    {"string",      "str",      "",         "10"},  // First item is the pkey
    {"string2",     "str",      "Pepe",     "20"},
    {"integer",     "int",      "0660",     "8"},   // beginning with "0":octal,"x":hexa, others: integer
    {"boolean",     "bool",     "false",    "8"},
    {0}   // HACK important, final null
};

 ***************************************************************************/
typedef struct {
    const char *name;
    const char *type;   // type can be: "str", "int", "real", "bool", "null", "dict", "list"
    const char *defaults;
    const char *fillspace;
} json_desc_t;

PUBLIC json_t *create_json_record(
    const json_desc_t *json_desc
);

/***************************************************************************
 *
 *  Convert a json record desc to a topic schema
 *
    json_record
    {
        name: string
        type: string
        defaults: string
        fillspace: string
    }

    schema
    {
        id: string
        header: string
        type: string
        fillspace: integer
    }

 ***************************************************************************/
PUBLIC json_t *json_record_to_schema(const json_desc_t *json_desc);

/*
 *  If exclusive then let file opened and return the fd, else close the file
 */
PUBLIC json_t *load_persistent_json(
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error,
    int *pfd,
    BOOL exclusive,
    BOOL silence  // HACK to silence TRUE you MUST set on_critical_error=LOG_NONE
);

PUBLIC json_t *load_json_from_file(
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error
);

PUBLIC int save_json_to_file(
    const char *directory,
    const char *filename, // Can't be empty
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,        // Create file if not exists or overwrite.
    BOOL only_read,
    json_t *jn_data     // owned
);

/**rst**
    Check all refcounts
**rst**/
PUBLIC int json_check_refcounts(
    json_t *kw, // not owned
    int max_refcount,
    int *result // firstly initalize to 0
);

/**rst**
    Check deeply the refcount of kw
**rst**/
PUBLIC int json_print_refcounts(
    json_t *jn, // not owned
    int level
);



#ifdef __cplusplus
}
#endif
