/***********************************************************************
 *          KW.C
 *          Kw functions
 *          Copyright (c) 2013-2015 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "14_kw.h"

/****************************************************************
 *         Constants
 ****************************************************************/
#define MAX_SERIALIZED_FIELDS 20


/****************************************************************
 *         Structures
 ****************************************************************/
typedef struct serialize_fields_s {
    const char *binary_field_name;
    const char *serialized_field_name;
    serialize_fn_t serialize_fn;
    deserialize_fn_t deserialize_fn;
    incref_fn_t incref_fn;
    decref_fn_t decref_fn;
} serialize_fields_t;


/****************************************************************
 *         Data
 ****************************************************************/
PRIVATE char delimiter[2] = {'`',0};
PRIVATE int max_slot = 0;
PRIVATE serialize_fields_t serialize_fields[MAX_SERIALIZED_FIELDS+1];


/****************************************************************
 *         Prototypes
 ****************************************************************/
PRIVATE json_t * _duplicate_array(json_t *kw, const char **keys, int underscores, BOOL serialize);
PRIVATE json_t * _duplicate_object(json_t *kw, const char **keys, int underscores, BOOL serialize);
PRIVATE int _walk_object(
    json_t *kw,
    int (*callback)(json_t *kw, const char *key, json_t *value)
);
PRIVATE json_t *_kw_search_dict(json_t *kw, const char *path, kw_flag_t flag);
PRIVATE json_t *_kw_find_path(json_t *kw, const char *path, BOOL verbose);


/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int kw_add_binary_type(
    const char *binary_field_name,
    const char *serialized_field_name,
    serialize_fn_t serialize_fn,
    deserialize_fn_t deserialize_fn,
    incref_fn_t incref_fn,
    decref_fn_t decref_fn)
{
    if(max_slot >= MAX_SERIALIZED_FIELDS) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "table serialize fields FULL",
            NULL
        );
        return -1;
    }
    serialize_fields_t * pf = serialize_fields + max_slot;
    pf->binary_field_name = binary_field_name;
    pf->serialized_field_name = serialized_field_name;
    pf->serialize_fn = serialize_fn;
    pf->deserialize_fn = deserialize_fn;
    pf->incref_fn = incref_fn;
    pf->decref_fn = decref_fn;

    max_slot++;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void kw_clean_binary_types(void)
{
    max_slot = 0;
}

/***************************************************************************
 *  Serialize fields
 ***************************************************************************/
PUBLIC json_t *kw_serialize(
    json_t *kw_ // owned
)
{
    json_t *kw = kw_duplicate(kw_);
    serialize_fields_t * pf = serialize_fields;
    while(pf->binary_field_name) {
        if(kw_has_key(kw, pf->binary_field_name)) {
            /*
             *  Pop the binary field from kw
             */
            void *binary = (void *)(size_t)kw_get_int(
                kw,
                pf->binary_field_name,
                0,
                0
            );

            /*
             *  Serialize
             */
            if(pf->serialize_fn) {
                json_t *jn_serialized = pf->serialize_fn(binary);

                /*
                 *  Decref binary
                 */
                if(pf->decref_fn) {
                    pf->decref_fn(binary);
                }

                /*
                 *  Save the serialized json field to kw
                 */
                if(jn_serialized) {
                    json_object_set_new(
                        kw,
                        pf->serialized_field_name,
                        jn_serialized
                    );
                } else {
                    log_error(LOG_OPT_TRACE_STACK,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "serialize_fn() FAILED",
                        "key",          "%s", pf->binary_field_name,
                        NULL
                    );
                }
            }
            json_object_del(kw, pf->binary_field_name);
        }
        pf++;
    }
    KW_DECREF(kw_);
    return kw;
}

/***************************************************************************
 *  Deserialize fields
 ***************************************************************************/
PUBLIC json_t *kw_deserialize(
    json_t *kw_ // owned
)
{
    json_t *kw = kw_duplicate(kw_);

    serialize_fields_t * pf = serialize_fields;
    while(pf->serialized_field_name) {
        if(kw_has_key(kw, pf->serialized_field_name)) {
            /*
             *  Pop the serialized json field from kw
             */
            json_t *jn_serialized = kw_get_dict(
                kw,
                pf->serialized_field_name,
                0,
                0
            );

            /*
             *  Deserialize
             */
            if(pf->deserialize_fn) {
                void *binary = pf->deserialize_fn(jn_serialized);

                /*
                 *  Decref json
                 */
                JSON_DECREF(jn_serialized);

                /*
                 *  Save the binary field to kw
                 */
                json_object_set_new(
                    kw,
                    pf->binary_field_name,
                    json_integer((json_int_t)(size_t)binary)
                );
            }
            json_object_del(kw, pf->serialized_field_name);

        }
        pf++;
    }
    KW_DECREF(kw_);
    return kw;
}

/***************************************************************************
 *  Incref json kw and his binary fields
 ***************************************************************************/
PUBLIC json_t *kw_incref(json_t *kw)
{
    if(!kw) {
        return 0;
    }
    if(kw->refcount <= 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD kw_incref()",
            NULL
        );
        return 0;
    }

    serialize_fields_t * pf = serialize_fields;
    while(pf->binary_field_name) {
        if(kw_has_key(kw, pf->binary_field_name)) {
            /*
             *  Get the binary field from kw
             */
            void *binary = (void *)(size_t)kw_get_int(
                kw,
                pf->binary_field_name,
                0,
                0
            );

            if(binary) {
                /*
                *  Incref binary
                */
                if(pf->incref_fn) {
                    pf->incref_fn(binary);
                }
            }
        }
        pf++;
    }
    json_incref(kw);
    return kw;
}

/***************************************************************************
 *  Decref json kw and his binary fields
 ***************************************************************************/
PUBLIC json_t *kw_decref(json_t* kw)
{
    if(!kw) {
        return 0;
    }
    if(kw->refcount <= 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD kw_decref()",
            NULL
        );
        return 0;
    }

    serialize_fields_t * pf = serialize_fields;
    while(pf->binary_field_name) {
        if(kw_has_key(kw, pf->binary_field_name)) {
            /*
             *  Get the binary field from kw
             */
            void *binary = (void *)(size_t)kw_get_int(
                kw,
                pf->binary_field_name,
                0,
                0
            );
            if(binary) {
                /*
                *  Incref binary
                */
                if(pf->decref_fn) {
                    pf->decref_fn(binary);
                }
            }
        }
        pf++;
    }
    json_decref(kw);
    if(kw->refcount <= 0) {
        kw = 0;
    }
    return kw;
}

/***************************************************************************
 *  Search delimiter
 ***************************************************************************/
PRIVATE char *search_delimiter(const char *s, char delimiter_)
{
    if(!delimiter_) {
        return 0;
    }
    return strchr(s, delimiter_);
}

/***************************************************************************
 *  Return the dict searched by path
 *  All in path must be dict!
 ***************************************************************************/
PRIVATE json_t *_kw_search_dict(json_t *kw, const char *path, kw_flag_t flag)
{
    if(!path) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path NULL",
            NULL
        );
        return 0;
    }

    char *p = search_delimiter(path, delimiter[0]);
    if(!p) {
        // Last subdict
        json_t *leaf_dict = json_object_get(kw, path);
        if(!leaf_dict && (flag & KW_CREATE) && kw) {
            leaf_dict = json_object();
            json_object_set_new(kw, path, leaf_dict);
        }
        return leaf_dict;
    }
    if(!json_is_object(kw)) {
        // silence
        return 0;
    }

    char segment[256];
    if(snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path)>=sizeof(segment)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "buffer too small",
            "path",         "%s", path,
            NULL
        );
    }

    json_t *node_dict = json_object_get(kw, segment);
    if(!node_dict && (flag & KW_CREATE) && kw) {
        node_dict = json_object();
        json_object_set_new(kw, segment, node_dict);
    }
    return _kw_search_dict(node_dict, p+1, flag);
}

/***************************************************************************
 *  Return the json's value find by path
 *  Walk over dicts and lists
 ***************************************************************************/
PRIVATE json_t *_kw_find_path(json_t *kw, const char *path, BOOL verbose)
{
    if(!(json_is_object(kw) || json_is_array(kw))) {
        if(verbose) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "kw must be list or dict",
                NULL
            );
        }
        return 0;
    }
    if(!path) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path NULL",
            NULL
        );
        return 0;
    }
    if(kw->refcount <=0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json refcount 0",
            "path",         "%s", path,
            NULL
        );
        return 0;
    }

    char *p = search_delimiter(path, delimiter[0]);
    if(!p) {
        if(json_is_object(kw)) {
            // Dict
            json_t *value = json_object_get(kw, path);
            if(!value && verbose) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "path not found",
                    "path",         "%s", path,
                    NULL
                );
            }
            return value;

        } else {
            // Array
            int idx = atoi(path);
            json_t *value = json_array_get(kw, idx);
            if(!value && verbose) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "path not found",
                    "path",         "%s", path,
                    "idx",          "%d", idx,
                    NULL
                );
            }
            return value;
        }
    }

    char segment[256];
    if(snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path)>=sizeof(segment)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "buffer too small",
            "path",         "%s", path,
            NULL
        );
    }

    json_t *next_json = 0;
    if(json_is_object(kw)) {
        // Dict
        next_json = json_object_get(kw, segment);
        if(!next_json) {
            if(verbose) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Dict segment not found",
                    "path",         "%s", path,
                    "segment",      "%s", segment,
                    NULL
                );
            }
            return 0;
        }
    } else {
        // Array
        int idx = atoi(segment);
        next_json = json_array_get(kw, idx);
        if(!next_json) {
            if(verbose) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "List segment not found",
                    "path",         "%s", path,
                    "segment",      "%s", segment,
                    "idx",          "%d", idx,
                    NULL
                );
            }
            return 0;
        }
    }

    return _kw_find_path(next_json, p+1, verbose);
}

/***************************************************************************
    Return the json's value find by path, walking over lists and dicts
 ***************************************************************************/
PUBLIC json_t *kw_find_path(json_t *kw, const char *path, BOOL verbose)
{
    return _kw_find_path(kw, path, verbose);
}

/***************************************************************************
 *  Check if the dictionary ``kw`` has the key ``key``
 ***************************************************************************/
PUBLIC BOOL kw_has_key(json_t *kw, const char *key)
{
    if(!json_is_object(kw)) {
        return FALSE;
    }
    if(json_object_get(kw, key)) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *  Check if the dictionary ``path``  of ``kw`` has the key ``key``
 ***************************************************************************/
PUBLIC BOOL kw_has_subkey(json_t *kw, const char *path, const char *key)
{
    json_t *jn_dict = kw_get_dict(kw, path, 0, 0);
    if(!jn_dict) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subdict not found",
            "subdict",      "%s", path,
            NULL
        );
        return FALSE;
    }
    return kw_has_key(jn_dict, key);
}


/***************************************************************************
   Set delimiter. Default is '`'
 ***************************************************************************/
PUBLIC char kw_set_path_delimiter(char delimiter_)
{
    char old_delimiter = delimiter[0];
    delimiter[0] = delimiter_;
    return old_delimiter;
}

/***************************************************************************
   Return TRUE if the dictionary ``kw`` has the path ``path``.
 ***************************************************************************/
PUBLIC BOOL kw_has_path(json_t *kw, const char *path)
{
    if(_kw_find_path(kw, path, FALSE)) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *   Delete subkey
 ***************************************************************************/
PUBLIC int kw_delete_subkey(json_t *kw, const char *path, const char *key)
{
    json_t *jn_dict = kw_get_dict(kw, path, 0, 0);
    if(!jn_dict) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subdict not found",
            "subdict",      "%s", path,
            NULL
        );
        return -1;
    }
    if(!kw_has_key(jn_dict, key)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "key not found",
            "subdict",      "%s", path,
            "key",          "%s", key,
            NULL
        );
        log_debug_json(0, kw, "key not found");
        return -1;
    }
    return json_object_del(jn_dict, key);
}

/***************************************************************************
 *  Delete the dict's value searched by path
 ***************************************************************************/
PUBLIC int kw_delete(json_t *kw, const char *path)
{
    if(!json_is_object(kw)) {
        // silence
        return 0;
    }
    if(!path) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path NULL",
            NULL
        );
        return 0;
    }
    if(kw->refcount <=0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json refcount 0",
            "path",         "%s", path,
            NULL
        );
        return 0;
    }
    char *p = search_delimiter(path, delimiter[0]);
    if(!p) {
        json_t *jn_item = json_object_get(kw, path);
        if(jn_item) {
            return json_object_del(kw, path);
        }
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path not found",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }

    char segment[256];
    if(snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path)>=sizeof(segment)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "buffer too small",
            "path",         "%s", path,
            NULL
        );
    }

    json_t *deep_dict = json_object_get(kw, segment);
    return kw_delete(deep_dict, p+1);
}

/***************************************************************************
 *  Get an dict value from an json object searched by path
 ***************************************************************************/
PUBLIC json_t *kw_get_dict(
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag)
{
    json_t *jn_dict = _kw_find_path(kw, path, FALSE);
    if(!jn_dict) {
        if((flag & KW_CREATE) && default_value && kw) {
            kw_set_dict_value(kw, path, default_value);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!json_is_object(jn_dict)) {
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json dict, default value returned",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path '%s' MUST BE a json dict, default value returned", path);
        }
        return default_value;
    }
    JSON_DECREF(default_value);

    if(flag & KW_EXTRACT) {
        json_incref(jn_dict);
        kw_delete(kw, path);
    }

    return jn_dict;
}

/***************************************************************************
 *  Get an list value from an json object searched by path
 ***************************************************************************/
PUBLIC json_t *kw_get_list(
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag)
{
    json_t *jn_list = _kw_find_path(kw, path, FALSE);
    if(!jn_list) {
        if((flag & KW_CREATE) && default_value && kw) {
            kw_set_dict_value(kw, path, default_value);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!json_is_array(jn_list)) {
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json list, default value returned",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path '%s' MUST BE a json list, default value returned", path);
        }
        return default_value;
    }
    JSON_DECREF(default_value);

    if(flag & KW_EXTRACT) {
        json_incref(jn_list);
        kw_delete(kw, path);
    }

    return jn_list;
}

/***************************************************************************
 *  Get an int value from an json object searched by path
 ***************************************************************************/
PUBLIC json_int_t kw_get_int(
    json_t *kw,
    const char *path,
    json_int_t default_value,
    kw_flag_t flag)
{
    json_t *jn_int = _kw_find_path(kw, path, FALSE);
    if(!jn_int) {
        if((flag & KW_CREATE) && kw) {
            json_t *jn_new = json_integer(default_value);
            kw_set_dict_value(kw, path, jn_new);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!(json_is_real(jn_int) || json_is_integer(jn_int))) {
        if(!(flag & KW_WILD_NUMBER)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json integer",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path MUST BE a json integer '%s'", path);
            return default_value;
        }
    }

    json_int_t value = 0;
    if(json_is_integer(jn_int)) {
        value = json_integer_value(jn_int);
    } else if(json_is_real(jn_int)) {
        value = (json_int_t)json_real_value(jn_int);
    } else if(json_is_boolean(jn_int)) {
        value = json_is_true(jn_int)?1:0;
    } else if(json_is_string(jn_int)) {
        const char *v = json_string_value(jn_int);
//         if(*v == '0') {
//             value = strtoll(v, 0, 8);
//         } else if(*v == 'x' || *v == 'X') {
//             value = strtoll(v, 0, 16);
//         } else {
//             value = strtoll(v, 0, 10);
//         }
        value = strtoll(v, 0, 0); // WARNING change by this: with base 0 strtoll() does all
    } else if(json_is_null(jn_int)) {
        value = 0;
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path MUST BE a simple json element",
            "path",         "%s", path,
            NULL
        );
        log_debug_json(0, kw, "path MUST BE a simple json element '%s'", path);
        return 0;
    }
    if(flag & KW_EXTRACT) {
        kw_delete(kw, path);
    }
    return value;
}

/***************************************************************************
 *  Get an real value from an json object searched by path
 ***************************************************************************/
PUBLIC double kw_get_real(
    json_t *kw,
    const char *path,
    double default_value,
    kw_flag_t flag)
{
    json_t *jn_real = _kw_find_path(kw, path, FALSE);
    if(!jn_real) {
        if((flag & KW_CREATE) && kw) {
            json_t *jn_new = json_real(default_value);
            kw_set_dict_value(kw, path, jn_new);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!(json_is_real(jn_real) || json_is_integer(jn_real))) {
        if(!(flag & KW_WILD_NUMBER)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json real",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path MUST BE a json real '%s'", path);
            return default_value;
        }
    }

    double value = 0;
    if(json_is_real(jn_real)) {
        value = json_real_value(jn_real);
    } else if(json_is_integer(jn_real)) {
        value = json_integer_value(jn_real);
    } else if(json_is_boolean(jn_real)) {
        value = json_is_true(jn_real)?1.0:0.0;
    } else if(json_is_string(jn_real)) {
        value = atof(json_string_value(jn_real));
    } else if(json_is_null(jn_real)) {
        value = 0;
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path MUST BE a simple json element",
            "path",         "%s", path,
            NULL
        );
        log_debug_json(0, kw, "path MUST BE a simple json element '%s'", path);
        return 0;
    }
    if(flag & KW_EXTRACT) {
        kw_delete(kw, path);
    }
    return value;
}

/***************************************************************************
 *  Get a bool value from an json object searched by path
 ***************************************************************************/
PUBLIC BOOL kw_get_bool(
    json_t *kw,
    const char *path,
    BOOL default_value,
    kw_flag_t flag)
{
    json_t *jn_bool = _kw_find_path(kw, path, FALSE);
    if(!jn_bool) {
        if((flag & KW_CREATE) && kw) {
            json_t *jn_new = json_boolean(default_value);
            kw_set_dict_value(kw, path, jn_new);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!json_is_boolean(jn_bool)) {
        if(!(flag & KW_WILD_NUMBER)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json boolean",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path MUST BE a json boolean '%s'", path);
            return default_value;
        }
    }

    BOOL value = 0;
    if(json_is_boolean(jn_bool)) {
        value = json_is_true(jn_bool)?1:0;
    } else if(json_is_integer(jn_bool)) {
        value = json_integer_value(jn_bool)?1:0;
    } else if(json_is_real(jn_bool)) {
        value = json_real_value(jn_bool)?1:0;
    } else if(json_is_string(jn_bool)) {
        const char *v = json_string_value(jn_bool);
        if(strcasecmp(v, "true")==0) {
            value = 1;
        } else if(strcasecmp(v, "false")==0) {
            value = 0;
        } else {
            value = atoi(v)?1:0;
        }
    } else if(json_is_null(jn_bool)) {
        value = 0;
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path MUST BE a simple json element",
            "path",         "%s", path,
            NULL
        );
        log_debug_json(0, kw, "path MUST BE a simple json element '%s'", path);
        return 0;
    }
    if(flag & KW_EXTRACT) {
        kw_delete(kw, path);
    }
    return value;
}

/***************************************************************************
 *  Get a string value from an json object searched by path
 ***************************************************************************/
PUBLIC const char *kw_get_str(
    json_t *kw,
    const char *path,
    const char *default_value,
    kw_flag_t flag)
{
    json_t *jn_str = _kw_find_path(kw, path, FALSE);
    if(!jn_str) {
        if((flag & KW_CREATE) && kw) {
            json_t *jn_new;
            if(!default_value) {
                jn_new = json_null();
            } else {
                jn_new = json_string(default_value);
            }
            kw_set_dict_value(kw, path, jn_new);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!json_is_string(jn_str)) {
        if(!json_is_null(jn_str)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json str",
                "path",         "%s", path,
                NULL
            );
            log_debug_json(0, kw, "path MUST BE a json str");
        }
        return default_value;
    }

    if(flag & KW_EXTRACT) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot extract a string",
            "path",         "%s", path,
            NULL
        );
    }

    const char *str = json_string_value(jn_str);
    return str;
}

/***************************************************************************
 *  Get any value from an json object searched by path
 ***************************************************************************/
PUBLIC json_t *kw_get_dict_value(
    json_t *kw,
    const char *path,
    json_t *default_value,  // owned
    kw_flag_t flag)
{
    json_t *jn_value = _kw_find_path(kw, path, FALSE);
    if(!jn_value) {
        if((flag & KW_CREATE) && default_value && kw) {
            kw_set_dict_value(kw, path, default_value);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
                );
            log_debug_json(0, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    JSON_DECREF(default_value);

    if(flag & KW_EXTRACT) {
        json_incref(jn_value);
        kw_delete(kw, path);
    }
    return jn_value;
}

/***************************************************************************
 *  Like json_object_set but with a path.
 ***************************************************************************/
PUBLIC int kw_set_dict_value(
    json_t *kw,
    const char *path,   // The last word after . is the key
    json_t *value) // owned
{
    if(!json_is_object(kw)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json object",
            "path",         "%s", path,
            NULL
        );
        JSON_DECREF(value);
        return -1;
    }

    char *p = search_delimiter(path, delimiter[0]);
    if(p) {
        char segment[1024];
        if(snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path)>=sizeof(segment)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "buffer too small",
                "path",         "%s", path,
                NULL
            );
        }

        if(empty_string(segment)) {
            return kw_set_dict_value(kw, p+1, value);
        }
        json_t *node_dict = json_object_get(kw, segment);
        if(node_dict) {
            return kw_set_dict_value(node_dict, p+1, value);
        } else {
            node_dict = json_object();
            json_object_set_new(kw, segment, node_dict);
            return kw_set_dict_value(node_dict, p+1, value);
        }
    }
    int ret = json_object_set_new(kw, path, value);
    if(ret < 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json_object_set_new() FAILED",
            "path",         "%s", path,
            NULL
        );
        log_debug_json(0, value, "json_object_set_new() FAILED");
    }
    return ret;
}

/***************************************************************************
 *  Get any value from an json object searched by path and subdict
 ***************************************************************************/
PUBLIC json_t *kw_get_subdict_value(
    json_t *kw,
    const char *path,
    const char *key,
    json_t *jn_default_value,  // owned
    kw_flag_t flag)
{
    json_t *jn_subdict = _kw_search_dict(kw, path, flag);
    if(!jn_subdict) {
        return jn_default_value;
    }
    if(empty_string(key)) {
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "key NULL",
                "path",         "%s", path,
                NULL
            );
        }
        return jn_default_value;
    }
    return kw_get_dict_value(jn_subdict, key, jn_default_value, flag);
}

/***************************************************************************
 *  Like json_object_set but with a path and subdict.
 ***************************************************************************/
PUBLIC int kw_set_subdict_value(
    json_t* kw,
    const char *path,
    const char *key,
    json_t *value) // owned
{
    json_t *jn_subdict = _kw_search_dict(kw, path, KW_CREATE);
    if(!jn_subdict) {
        // Error already logged
        KW_DECREF(value);
        return -1;
    }
    if(json_object_set_new(jn_subdict, key, value)<0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json_object_set_new() FAILED",
            "path",         "%s", path,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *kw_get_list_value(
    json_t *kw,
    int idx,
    kw_flag_t flag)
{
    if(!json_is_array(kw)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array",
            NULL
        );
        return 0;
    }
    if(idx >= json_array_size(kw)) {
        if(flag & KW_REQUIRED) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "list idx NOT FOUND",
                "idx",          "%d", (int)idx,
                "array_size",   "%d", (int)json_array_size(kw),
                NULL
            );
        }
        return 0;
    }

    json_t *v = json_array_get(kw, idx);
    if(v && (flag & KW_EXTRACT)) {
        JSON_INCREF(v);
        json_array_remove(kw, idx);
    }
    return v;
}

/***************************************************************************
 *  Get a string value from an json list search by idx
 ***************************************************************************/
PUBLIC const char *kw_get_list_str(json_t *list, int idx)
{
    json_t *jn_str;
    const char *str;

    if(!json_is_array(list)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        log_debug_json(0, list, "list MUST BE a json array");
        return 0;
    }

    jn_str = json_array_get(list, idx);
    if(jn_str && !json_is_string(jn_str)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "value MUST BE a json string",
            NULL
        );
        return 0;
    }

    str = json_string_value(jn_str);
    return str;
}

/***************************************************************************
 *  like json_array_append but with a path into a dict
 ***************************************************************************/
PUBLIC int kw_list_append(json_t *kw, const char *path, json_t *value)
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

    char *p = search_delimiter(path, delimiter[0]);
    if(p) {
        char segment[1024];
        if(snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path)>=sizeof(segment)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "buffer too small",
                "path",         "%s", path,
                NULL
            );
        }

        json_t *deep_dict = json_object_get(kw, segment);
        if(json_is_object(deep_dict)) {
            return kw_list_append(deep_dict, p+1, value);
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
 *  Get a C binary int array from json int list
 *  The returned value must be free with gbmem_free()
 ***************************************************************************/
PUBLIC uint32_t * kw_get_bin_uint32_array(
    json_t *kw,
    const char *path,
    size_t *uint32_array_len)
{
    json_t *jn_list = kw_get_list(kw, path, 0, FALSE);
    if(!jn_list) {
        if(uint32_array_len)
            *uint32_array_len = 0;
        return 0;
    }

    size_t items = json_array_size(jn_list);
    if(items<=0) {
        if(uint32_array_len)
            *uint32_array_len = 0;
        return 0;
    }
    uint32_t *uint32_array = gbmem_malloc(sizeof(json_int_t) * items);
    if(!uint32_array) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory for int_array",
            "size",         "%d", sizeof(json_int_t) * items,
            NULL);
        if(uint32_array_len)
            *uint32_array_len = 0;
        return 0;
    }
    for(int i=0; i<items; i++) {
        json_t *it = json_array_get(jn_list, i);
        if(!json_is_integer(it)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON_ERROR,
                "msg",          "%s", "no integer item in int list",
                "idx",          "%d", i,
                NULL
            );
            continue;
        }
        uint32_array[i] = json_integer_value(it);
    }
    if(uint32_array_len)
        *uint32_array_len = items;
    return uint32_array;
}

/***************************************************************************
    Utility for databases.
    Extract (remove) 'ids' or 'id' from kw
    Return a new dict only with {"ids": [json_expand_integer_list(ids) | id]}
 ***************************************************************************/
PUBLIC json_t *kwids_extract_and_expand_ids(json_t *kw) // not owned
{
    if(!kw) {
        return 0;
    }
    json_t *kw_ids = json_object();
    if(kw_has_key(kw, "ids")) {
        json_t *jn_ids = kw_get_dict_value(kw, "ids", 0, 0);
        json_object_set_new(kw_ids, "ids", json_expand_integer_list(jn_ids));
        json_object_del(kw, "ids");
    } else if(kw_has_key(kw, "id")) {
        json_t *jn_id = kw_get_dict_value(kw, "id", 0, 0);
        json_object_set_new(kw_ids, "ids", json_expand_integer_list(jn_id));
        json_object_del(kw, "id");
    } else {
        JSON_DECREF(kw_ids);
        return 0;
    }
    return kw_ids;
}

/***************************************************************************
    Utility for databases.
    Same as kw_extract_and_expand_ids()
    but it's required that must be only one id, otherwise return 0.
    Return a new dict only with {"ids": [ids[0] | id]}
 ***************************************************************************/
PUBLIC json_t * kwids_extract_id(json_t *kw) // not owned
{
    json_t *kw_ids = kwids_extract_and_expand_ids(kw);
    if(!kw_ids) {
        return 0;
    }
    if(json_array_size(kw_get_dict_value(kw_ids, "ids", 0, 0))!=1) {
        JSON_DECREF(kw_ids);
        return 0;
    }
    return kw_ids;
}

/***************************************************************************
    Utility for databases.
    Return dict with {"ids": [json_expand_integer_list(jn_id)]}
 ***************************************************************************/
PUBLIC json_t *kwids_json2kwids(json_t *jn_ids)  // not owned
{
    if(!jn_ids) {
        return 0;
    }
    json_t *kw_ids = json_object();
    json_object_set_new(kw_ids, "ids", json_expand_integer_list(jn_ids));
    return kw_ids;
}

/***************************************************************************
    Utility for databases.
    Return dict with {"ids": [id]}
    If id is 0 then return 0 (id cannot be 0)
 ***************************************************************************/
PUBLIC json_t *kwids_id2kwids(json_int_t id)
{
    if(id == 0) {
        return 0;
    }
    json_t *kw_ids = json_object();
    json_t *jn_list = json_array();
    json_array_append_new(jn_list, json_integer(id));
    json_object_set_new(kw_ids, "ids", jn_list);
    return kw_ids;

}

/***************************************************************************
 *  Simple json to real
 ***************************************************************************/
PUBLIC double jn2real(json_t *jn_var)
{
    double val = 0.0;
    if(json_is_real(jn_var)) {
        val = json_real_value(jn_var);
    } else if(json_is_integer(jn_var)) {
        val = (double)json_integer_value(jn_var);
    } else if(json_is_string(jn_var)) {
        const char *s = json_string_value(jn_var);
        val = atof(s);
    } else if(json_is_true(jn_var)) {
        val = 1.0;
    } else if(json_is_false(jn_var)) {
        val = 0.0;
    } else if(json_is_null(jn_var)) {
        val = 0.0;
    }
    return val;
}

/***************************************************************************
 *  Simple json to int
 ***************************************************************************/
PUBLIC json_int_t jn2integer(json_t *jn_var)
{
    json_int_t val = 0;
    if(json_is_real(jn_var)) {
        val = (json_int_t)json_real_value(jn_var);
    } else if(json_is_integer(jn_var)) {
        val = json_integer_value(jn_var);
    } else if(json_is_string(jn_var)) {
        const char *v = json_string_value(jn_var);
        if(*v == '0') {
            val = strtoll(v, 0, 8);
        } else if(*v == 'x' || *v == 'X') {
            val = strtoll(v, 0, 16);
        } else {
            val = strtoll(v, 0, 10);
        }
    } else if(json_is_true(jn_var)) {
        val = 1;
    } else if(json_is_false(jn_var)) {
        val = 0;
    } else if(json_is_null(jn_var)) {
        val = 0;
    }
    return val;
}

/***************************************************************************
 *  Simple json to string, WARNING free return with gbmem_free
 ***************************************************************************/
PUBLIC char *jn2string(json_t *jn_var)
{
    char temp[PATH_MAX];
    char *s="";

    if(json_is_string(jn_var)) {
        s = (char *)json_string_value(jn_var);
    } else if(json_is_integer(jn_var)) {
        json_int_t v = json_integer_value(jn_var);
        snprintf(temp, sizeof(temp), "%"JSON_INTEGER_FORMAT, v);
        s = temp;
    } else if(json_is_real(jn_var)) {
        double v = json_real_value(jn_var);
        snprintf(temp, sizeof(temp), "%.17f", v);
        s = temp;
    } else if(json_is_boolean(jn_var)) {
        s = json_is_true(jn_var)?"1":"0";
    }

    return gbmem_strdup(s);
}

/***************************************************************************
 *  Simple json to bool
 ***************************************************************************/
PUBLIC BOOL jn2bool(json_t *jn_var)
{
    BOOL val = 0;
    if(json_is_real(jn_var)) {
        val = json_real_value(jn_var)?1:0;
    } else if(json_is_integer(jn_var)) {
        val = json_integer_value(jn_var)?1:0;
    } else if(json_is_string(jn_var)) {
        const char *v = json_string_value(jn_var);
        val = empty_string(v)?0:1;
    } else if(json_is_true(jn_var)) {
        val = 1;
    } else if(json_is_false(jn_var)) {
        val = 0;
    } else if(json_is_null(jn_var)) {
        val = 0;
    }
    return val;
}

/***************************************************************************
 *  if binary is inside of kw, incref binary
 ***************************************************************************/
PRIVATE serialize_fields_t * get_serialize_field(const char *binary_field_name)
{
    serialize_fields_t * pf = serialize_fields;
    while(pf->binary_field_name) {
        if(strcmp(pf->binary_field_name, binary_field_name)==0) {
            return pf;
        }
        pf++;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _walk_object(
    json_t *kw,
    int (*callback)(json_t *kw, const char *key, json_t *value)
)
{
    const char *key;
    json_t *value;
    json_object_foreach(kw, key, value) {
        json_type type = json_typeof(value);
        switch(type) {
        case JSON_OBJECT:
            if(_walk_object(value, callback)<0) {
                return -1;
            }
            break;
        case JSON_ARRAY:
        default:
            if(callback(kw, key, value)<0) {
                return -1;
            }
            break;
        }
    }

    return 0;
}

/***************************************************************************
 *  Walk over an object
 ***************************************************************************/
PUBLIC int kw_walk(
    json_t *kw, // not owned
    int (*callback)(json_t *kw, const char *key, json_t *value)
)
{
    if(json_is_object(kw)) {
        return _walk_object(kw, callback);
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json to duplicate must be an object or array",
            NULL
        );
        return -1;
    }
}

/***************************************************************************
 *  Make a twin of a array
 ***************************************************************************/
PRIVATE json_t * _duplicate_array(json_t *kw, const char **keys, int underscores, BOOL serialize)
{
    json_t *kw_dup_ = json_array();

    size_t idx;
    json_t *value;
    json_array_foreach(kw, idx, value) {
        json_t *new_value;
        json_type type = json_typeof(value);
        switch(type) {
        case JSON_OBJECT:
            new_value = _duplicate_object(value, keys, underscores, serialize);
            break;
        case JSON_ARRAY:
            new_value = _duplicate_array(value, keys, underscores, serialize);
            break;
        case JSON_STRING:
            new_value = json_string(json_string_value(value));
            break;
        case JSON_INTEGER:
            {
                /*
                 *  WARNING binary fields in array cannot be managed!
                 */
                json_int_t binary = json_integer_value(value);
                new_value = json_integer(binary);
            }
            break;
        case JSON_REAL:
            new_value = json_real(json_real_value(value));
            break;
        case JSON_TRUE:
            new_value = json_true();
            break;
        case JSON_FALSE:
            new_value = json_false();
            break;
        case JSON_NULL:
            new_value = json_null();
            break;
        }
        json_array_append_new(kw_dup_, new_value);
    }

    return kw_dup_;
}
/***************************************************************************
 *  Make a twin of a object
 ***************************************************************************/
PRIVATE json_t * _duplicate_object(json_t *kw, const char **keys, int underscores, BOOL serialize)
{
    json_t *kw_dup_ = json_object();

    const char *key;
    json_t *value;
    json_object_foreach(kw, key, value) {
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
        if(keys && !str_in_list(keys, key, FALSE)) {
            continue;
        }
        json_t *new_value;
        json_type type = json_typeof(value);
        switch(type) {
        case JSON_OBJECT:
            new_value = _duplicate_object(value, keys, underscores, serialize);
            break;
        case JSON_ARRAY:
            new_value = _duplicate_array(value, keys, underscores, serialize);
            break;
        case JSON_STRING:
            new_value = json_string(json_string_value(value));
            break;
        case JSON_INTEGER:
            {
                json_int_t binary = json_integer_value(value);
                new_value = json_integer(binary);
                if(serialize) {
                    serialize_fields_t * pf = get_serialize_field(key);
                    if(pf) {
                        pf->incref_fn((void *)(size_t)binary);
                    }
                }
            }
            break;
        case JSON_REAL:
            new_value = json_real(json_real_value(value));
            break;
        case JSON_TRUE:
            new_value = json_true();
            break;
        case JSON_FALSE:
            new_value = json_false();
            break;
        case JSON_NULL:
            new_value = json_null();
            break;
        }
        json_object_set_new(kw_dup_, key, new_value);
    }

    return kw_dup_;
}

/***************************************************************************
 *  Make a duplicate of kw
 *  WARNING clone of json_deep_copy(), but processing serialized fields
 ***************************************************************************/
PUBLIC json_t *kw_duplicate(json_t *kw) // not owned
{
    if(json_is_object(kw)) {
        return _duplicate_object(kw, 0, 0, TRUE);
    } else if(json_is_array(kw)) {
        return _duplicate_array(kw, 0, 0, TRUE);
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json to duplicate must be an object or array",
            NULL
        );
        return 0;
    }
}

/***************************************************************************
    HACK Convention: private data begins with "_".
    This function return a duplicate of kw removing all private data
 ***************************************************************************/
PUBLIC json_t *kw_filter_private(
    json_t *kw  // owned
)
{
    json_t *new_kw = 0;
    if(json_is_object(kw)) {
        new_kw =  _duplicate_object(kw, 0, 1, TRUE);
    } else if(json_is_array(kw)) {
        new_kw = _duplicate_array(kw, 0, 1, TRUE);
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json to duplicate must be an object or array",
            NULL
        );
    }
    KW_DECREF(kw);
    return new_kw;
}

/***************************************************************************
    HACK Convention: all metadata begins with "__".
    This function return a duplicate of kw removing all metadata
 ***************************************************************************/
PUBLIC json_t *kw_filter_metadata(
    json_t *kw  // owned
)
{
    json_t *new_kw = 0;
    if(json_is_object(kw)) {
        new_kw =  _duplicate_object(kw, 0, 2, TRUE);
    } else if(json_is_array(kw)) {
        new_kw = _duplicate_array(kw, 0, 2, TRUE);
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json to duplicate must be an object or array",
            NULL
        );
    }
    KW_DECREF(kw);
    return new_kw;
}

/***************************************************************************
    HACK Convention: private data begins with "_".
    Delete private keys
 ***************************************************************************/
PUBLIC int kw_delete_private_keys(
    json_t *kw  // NOT owned
)
{
    int underscores = 1;

    const char *key;
    json_t *value;
    void *n;
    json_object_foreach_safe(kw, n, key, value) {
        if(underscores) {
            int u;
            for(u=0; u<strlen(key); u++) {
                if(key[u] != '_') {
                    break;
                }
            }
            if(u == underscores) {
                json_object_del(kw, key);
            }
        }
    }

    return 0;
}

/***************************************************************************
    HACK Convention: metadata begins with "__".
    Delete metadata keys
 ***************************************************************************/
PUBLIC int kw_delete_metadata_keys(
    json_t *kw  // NOT owned
)
{
    int underscores = 2;

    const char *key;
    json_t *value;
    void *n;
    json_object_foreach_safe(kw, n, key, value) {
        if(underscores) {
            int u;
            for(u=0; u<strlen(key); u++) {
                if(key[u] != '_') {
                    break;
                }
            }
            if(u == underscores) {
                json_object_del(kw, key);
            }
        }
    }

    return 0;
}

/***************************************************************************
    Delete keys from kw
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

 ***************************************************************************/
PUBLIC int kw_delete_keys(
    json_t *kw,     // NOT owned
    json_t *keys,   // owned
    BOOL verbose
)
{
    if(json_is_string(keys)) {
        const char *key = json_string_value(keys);
        json_t *jn_value = kw_get_dict_value(kw, key, 0, 0);
        if(jn_value) {
            json_object_del(kw, key);
        } else {
            if(verbose) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "key not found",
                    "key",          "%s", key,
                    NULL
                );
            }
        }
    } else if(json_is_object(keys)) {
        const char *key;
        json_t *jn_v;
        json_object_foreach(keys, key, jn_v) {
            json_t *jn_value = kw_get_dict_value(kw, key, 0, 0);
            if(jn_value) {
                json_object_del(kw, key);
            } else {
                if(verbose) {
                    log_error(LOG_OPT_TRACE_STACK,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    } else if(json_is_array(keys)) {
        int idx;
        json_t *jn_v;
        json_array_foreach(keys, idx, jn_v) {
            const char *key = json_string_value(jn_v);
            json_t *jn_value = kw_get_dict_value(kw, key, 0, 0);
            if(jn_value) {
                json_object_del(kw, key);
            } else {
                if(verbose) {
                    log_error(LOG_OPT_TRACE_STACK,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    }

    KW_DECREF(keys);
    return 0;
}

/***************************************************************************
    Return a new kw only with the filter keys.
    If `keys` is null then return a clone of kw.
    A key can be repeated by the tree.
 ***************************************************************************/
PUBLIC json_t *kw_duplicate_with_only_keys(
    json_t *kw,         // not owned
    const char **keys
)
{
    if(json_is_object(kw)) {
        return _duplicate_object(kw, keys, 0, FALSE);
    } else if(json_is_array(kw)) {
        return _duplicate_array(kw, keys, 0, FALSE);
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json to duplicate (with only keys) must be an object or array",
            NULL
        );
        return 0;
    }
}

/***************************************************************************
    Update kw with other, except keys in except_keys
    First level only
 ***************************************************************************/
PUBLIC void kw_update_except(
    json_t *kw,  // not owned
    json_t *other,  // owned
    const char **except_keys
)
{
    const char *key;
    json_t *jn_value;
    json_object_foreach(other, key, jn_value) {
        if(str_in_list(except_keys, key, FALSE)) {
            continue;
        }
        json_object_set(kw, key, jn_value);
    }
}

/***************************************************************************
    Return a new kw only with the keys got by path.
    It's not a deep copy, new keys are the paths.
    Not valid with lists.
    If paths are empty return kw
 ***************************************************************************/
PUBLIC json_t *kw_clone_by_path(
    json_t *kw,         // owned
    const char **paths
)
{
    if(!paths || !*paths) {
        return kw;
    }
    json_t *kw_clone = json_object();
    while(*paths) {
        json_t *jn_value = kw_get_dict_value(kw, *paths, 0, 0);
        if(jn_value) {
            json_object_set(kw_clone, *paths, jn_value);
        }
        paths++;
    }
    KW_DECREF(kw);
    return kw_clone;
}

/***************************************************************************
    Return a new kw only with the keys got by dict's keys or list's keys (strings).
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

    It's not a deep copy, new keys are the paths.
    If paths are empty return kw
 ***************************************************************************/
PUBLIC json_t *kw_clone_by_keys(
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
)
{
    json_t *kw_clone = json_object();
    if(json_is_string(keys) && !empty_string(json_string_value(keys))) {
        const char *key = json_string_value(keys);
        json_t *jn_value = kw_get_dict_value(kw, key, 0, 0);
        if(jn_value) {
            json_object_set(kw_clone, key, jn_value);
        } else {
            if(verbose) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "key not found",
                    "key",          "%s", key,
                    NULL
                );
            }
        }
    } else if(json_is_object(keys) && json_object_size(keys)>0) {
        const char *key;
        json_t *jn_v;
        json_object_foreach(keys, key, jn_v) {
            json_t *jn_value = kw_get_dict_value(kw, key, 0, 0);
            if(jn_value) {
                json_object_set(kw_clone, key, jn_value);
            } else {
                if(verbose) {
                    log_error(LOG_OPT_TRACE_STACK,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    } else if(json_is_array(keys) && json_array_size(keys)>0) {
        int idx;
        json_t *jn_v;
        json_array_foreach(keys, idx, jn_v) {
            const char *key = json_string_value(jn_v);
            json_t *jn_value = kw_get_dict_value(kw, key, 0, 0);
            if(jn_value) {
                json_object_set(kw_clone, key, jn_value);
            } else {
                if(verbose) {
                    log_error(LOG_OPT_TRACE_STACK,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    } else {
        json_decref(kw_clone);
        KW_DECREF(keys);
        return kw;
    }

    KW_DECREF(keys);
    KW_DECREF(kw);
    return kw_clone;
}

/***************************************************************************
    Return a new kw except the keys got by dict's keys or list's keys (strings).
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

    It's not a deep copy, new keys are the paths.
    If paths are empty return empty
 ***************************************************************************/
PUBLIC json_t *kw_clone_by_not_keys(
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
)
{
    json_t *kw_clone = json_object();
    json_object_update(kw_clone, kw);

    if(json_is_string(keys) && !empty_string(json_string_value(keys))) {
        const char *key = json_string_value(keys);
        json_t *jn_value = kw_get_dict_value(kw, key, 0, 0);
        if(jn_value) {
            json_object_del(kw_clone, key);
        } else {
            if(verbose) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "key not found",
                    "key",          "%s", key,
                    NULL
                );
            }
        }
    } else if(json_is_object(keys) && json_object_size(keys)>0) {
        const char *key;
        json_t *jn_v;
        json_object_foreach(keys, key, jn_v) {
            json_t *jn_value = kw_get_dict_value(kw, key, 0, 0);
            if(jn_value) {
                json_object_del(kw_clone, key);
            } else {
                if(verbose) {
                    log_error(LOG_OPT_TRACE_STACK,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    } else if(json_is_array(keys) && json_array_size(keys)>0) {
        int idx;
        json_t *jn_v;
        json_array_foreach(keys, idx, jn_v) {
            const char *key = json_string_value(jn_v);
            json_t *jn_value = kw_get_dict_value(kw, key, 0, 0);
            if(jn_value) {
                json_object_del(kw_clone, key);
            } else {
                if(verbose) {
                    log_error(LOG_OPT_TRACE_STACK,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    } else {
        json_decref(kw_clone);
        KW_DECREF(keys);
        KW_DECREF(kw);
        return json_object();
    }

    KW_DECREF(keys);
    KW_DECREF(kw);
    return kw_clone;
}

/***************************************************************************
    Compare two json and return TRUE if they are identical.
 ***************************************************************************/
PUBLIC BOOL kw_is_identical(
    json_t *kw1,    // not owned
    json_t *kw2     // not owned
)
{
    if(!kw1 || !kw2) {
        return FALSE;
    }
    char *kw1_ = json2uglystr(kw1);
    char *kw2_ = json2uglystr(kw2);
    int ret = strcmp(kw1_, kw2_);
    gbmem_free(kw1_);
    gbmem_free(kw2_);
    return ret==0?TRUE:FALSE;
}

/***************************************************************************
    Only compare str/int/real/bool items
    Complex types are done as matched
    Return lower, iqual, higher (-1, 0, 1), like strcmp
 ***************************************************************************/
PUBLIC int cmp_two_simple_json(
    json_t *jn_var1,    // not owned
    json_t *jn_var2     // not owned
)
{
    /*
     *  Discard complex types, done as matched
     */
    if(json_is_object(jn_var1) ||
            json_is_object(jn_var2) ||
            json_is_array(jn_var1) ||
            json_is_array(jn_var2)) {
        return 0;
    }

    /*
     *  First try real
     */
    if(json_is_real(jn_var1) || json_is_real(jn_var2)) {
        double val1 = jn2real(jn_var1);
        double val2 = jn2real(jn_var2);
        if(val1 > val2) {
            return 1;
        } else if(val1 < val2) {
            return -1;
        } else {
            return 0;
        }
    }

    /*
     *  Try integer
     */
    if(json_is_integer(jn_var1) || json_is_integer(jn_var2)) {
        json_int_t val1 = jn2integer(jn_var1);
        json_int_t val2 = jn2integer(jn_var2);
        if(val1 > val2) {
            return 1;
        } else if(val1 < val2) {
            return -1;
        } else {
            return 0;
        }
    }

    /*
     *  Try boolean
     */
    if(json_is_boolean(jn_var1) || json_is_boolean(jn_var2)) {
        json_int_t val1 = jn2integer(jn_var1);
        json_int_t val2 = jn2integer(jn_var2);
        if(val1 > val2) {
            return 1;
        } else if(val1 < val2) {
            return -1;
        } else {
            return 0;
        }
    }

    /*
     *  Try string
     */
    char *val1 = jn2string(jn_var1);
    char *val2 = jn2string(jn_var2);
    int ret = strcmp(val1, val2);
    gbmem_free(val1);
    gbmem_free(val2);
    return ret;
}

/***************************************************************************
    Match a json dict with a json filter (only compare str/number)
 ***************************************************************************/
PRIVATE BOOL _kw_match_simple(
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    int level
)
{
    BOOL matched = FALSE;

    level++;

    if(json_is_array(jn_filter)) {
        // Empty array evaluate as false, until a match condition occurs.
        matched = FALSE;
        size_t idx;
        json_t *jn_filter_value;
        json_array_foreach(jn_filter, idx, jn_filter_value) {
            JSON_INCREF(jn_filter_value);
            matched = _kw_match_simple(
                kw,                 // not owned
                jn_filter_value,    // owned
                level
            );
            if(matched) {
                break;
            }
        }

    } else if(json_is_object(jn_filter)) {
        if(json_object_size(jn_filter)==0) {
            // Empty object evaluate as false.
            matched = FALSE;
        } else {
            // Not Empty object evaluate as true, until a NOT match condition occurs.
            matched = TRUE;
        }

        const char *filter_path;
        json_t *jn_filter_value;
        json_object_foreach(jn_filter, filter_path, jn_filter_value) {
            /*
             *  Variable compleja, recursivo
             */
            if(json_is_array(jn_filter_value) || json_is_object(jn_filter_value)) {
                JSON_INCREF(jn_filter_value);
                matched = _kw_match_simple(
                    kw,                 // not owned
                    jn_filter_value,    // owned
                    level
                );
                break;
            }

            /*
             *  Variable sencilla
             */
            /*
             * TODO get the name and op.
             */
            const char *path = filter_path; // TODO
            const char *op = "__equal__";

            /*
             *  Get the record value, firstly by path else by name
             */
            json_t *jn_record_value;
            // Firstly try the key as pointers
            jn_record_value = kw_get_dict_value(kw, path, 0, 0);
            if(!jn_record_value) {
                // Secondly try the key with points (.) as full key
                jn_record_value = json_object_get(kw, path);
            }
            if(!jn_record_value) {
                matched = FALSE;
                break;
            }

            /*
             *  Do simple operation
             */
            if(strcasecmp(op, "__equal__")==0) { // TODO __equal__ by default
                int cmp = cmp_two_simple_json(jn_record_value, jn_filter_value);
                if(cmp!=0) {
                    matched = FALSE;
                    break;
                }
            } else {
                // TODO op: __lower__ __higher__ __re__ __equal__
            }
        }
    }

    JSON_DECREF(jn_filter);
    return matched;
}

/***************************************************************************
    Match a json dict with a json filter (only compare str/number)
 ***************************************************************************/
PUBLIC BOOL kw_match_simple(
    json_t *kw,         // not owned
    json_t *jn_filter   // owned
)
{
    if(!jn_filter) {
        // Si no hay filtro pasan todos.
        return TRUE;
    }
    if(json_is_object(jn_filter) && json_object_size(jn_filter)==0) {
        // A empty object at first level evaluate as true.
        JSON_DECREF(jn_filter);
        return TRUE;
    }

    return _kw_match_simple(kw, jn_filter, 0);
}

/***************************************************************************
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of **duplicated** kw filtering the rows by `jn_filter` (where),
    and returning the `keys` fields of row (select).
    If match_fn is 0 then kw_match_simple is used.
 ***************************************************************************/
PUBLIC json_t *kw_select( // WARNING return **duplicated** objects
    json_t *kw,         // not owned
    const char **keys,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    if(!match_fn) {
        match_fn = kw_match_simple;
    }
    json_t *kw_new = json_array();

    if(json_is_array(kw)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw, idx, jn_value) {
            KW_INCREF(jn_filter);
            if(match_fn(jn_value, jn_filter)) {
                json_t *jn_row = kw_duplicate_with_only_keys(jn_value, keys);
                json_array_append_new(kw_new, jn_row);
            }
        }
    } else if(json_is_object(kw)) {
        KW_INCREF(jn_filter);
        if(match_fn(kw, jn_filter)) {
            json_t *jn_row = kw_duplicate_with_only_keys(kw, keys);
            json_array_append_new(kw_new, jn_row);
        }

    } else  {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array or object",
            NULL
        );
        return kw_new;
    }

    KW_DECREF(jn_filter);
    return kw_new;
}

/***************************************************************************
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of **duplicated** kw filtering the rows by `jn_filter` (where),
    and returning the `keys` fields of row (select).
    If match_fn is 0 then kw_match_simple is used.
 ***************************************************************************/
// TODO sin probar
PUBLIC json_t *kw_select2( // WARNING return **clone** objects
    json_t *kw,         // NOT owned
    json_t *jn_keys,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // NOT owned
        json_t *jn_filter   // owned
    )
)
{
    if(!match_fn) {
        match_fn = kw_match_simple;
    }
    json_t *kw_new = json_array();

    if(json_is_array(kw)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw, idx, jn_value) {
            KW_INCREF(jn_filter);
            if(match_fn(jn_value, jn_filter)) {
                json_t *jn_row = kw_clone_by_keys(jn_value, jn_keys, FALSE);
                json_array_append_new(kw_new, jn_row);
            }
        }
    } else if(json_is_object(kw)) {
        KW_INCREF(jn_filter);
        if(match_fn(kw, jn_filter)) {
            json_t *jn_row = kw_clone_by_keys(kw, jn_keys, FALSE);
            json_array_append_new(kw_new, jn_row);
        }

    } else  {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array or object",
            NULL
        );
        return kw_new;
    }

    KW_DECREF(jn_filter);
    return kw_new;
}

/***************************************************************************
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of incref (clone) kw filtering the rows by `jn_filter` (where),
    If match_fn is 0 then kw_match_simple is used.
    NOTE Using JSON_INCREF/JSON_DECREF
 ***************************************************************************/
PUBLIC json_t *kw_collect( // WARNING be care, you can modify the original records
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    if(!kw) {
        JSON_DECREF(jn_filter);
        // silence
        return 0;
    }
    if(!match_fn) {
        match_fn = kw_match_simple;
    }
    json_t *kw_new = json_array();
    if(json_is_array(kw)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw, idx, jn_value) {
            JSON_INCREF(jn_filter);
            if(match_fn(jn_value, jn_filter)) {
                json_array_append(kw_new, jn_value);
            }
        }
    } else if(json_is_object(kw)) {
        JSON_INCREF(jn_filter);
        if(match_fn(kw, jn_filter)) {
            json_array_append(kw_new, kw);
        }

    } else  {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array or object",
            NULL
        );
        JSON_DECREF(kw_new);
        return 0;
    }

    JSON_DECREF(jn_filter);
    return kw_new;
}

/***************************************************************************
    Navigate by deep dicts of kw.
    Arrays are navigated if contains `pkey` (id) field (string or integer ONLY).
 ***************************************************************************/
PRIVATE int _kw_navigate(
    const char *path,
    json_t *kw,         // not owned
    const char *pkey,
    int (*navigate_callback)(
        const char *subpath, json_t *subkw, void *param1, void *param2, void *param3
    ),
    void *param1,
    void *param2,
    void *param3
)
{
    if(json_is_object(kw)) {
        int ret = navigate_callback(path, kw, param1, param2, param3);
        if(ret < 0) {
            return ret;
        }
        const char *key;
        json_t *jn_value;
        json_object_foreach(kw, key, jn_value) {
            if(json_is_object(jn_value) || json_is_array(jn_value)) {
                char *path2 = str_concat3(path, delimiter, key);
                int ret = _kw_navigate(
                    path2,
                    jn_value,
                    pkey,
                    navigate_callback,
                    param1,param2,param3
                );
                str_concat_free(path2);
                if(ret < 0) {
                    return ret;
                }
            }
        }

    } else if(json_is_array(kw)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw, idx, jn_value) {
            /*
             * Check only objects inside arrays, and must contains `pkey` (id) field
             */
            if(json_is_object(jn_value)) {
                json_t *jn_id = json_object_get(jn_value, pkey);
                if(jn_id) {
                    const char *key;
                    char key_[80];
                    if(json_is_integer(jn_id)) {
                        snprintf(key_, sizeof(key_), "%"JSON_INTEGER_FORMAT, json_integer_value(jn_id));
                        key = key_;
                    } else if(json_is_string(jn_id)) {
                        key = json_string_value(jn_id);
                    } else {
                        continue;
                    }
                    char *path2 = str_concat3(path, delimiter, key);
                    int ret = _kw_navigate(
                        path2,
                        jn_value,
                        pkey,
                        navigate_callback,
                        param1,param2,param3
                    );
                    str_concat_free(path2);
                    if(ret < 0) {
                        return ret;
                    }
                }
            }
        }
    }

    return 0;
}

/***************************************************************************
    Navigate by deep dicts of kw.
    Arrays are navigated if contains `pkey` (id) field (string or integer ONLY).
 ***************************************************************************/
PUBLIC int kw_navigate(
    json_t *kw,         // not owned
    const char *pkey,
    int (*navigate_callback)(
        const char *path, json_t *kw, void *param1, void *param2, void *param3
    ),
    void *param1,
    void *param2,
    void *param3
)
{
    return _kw_navigate("", kw, pkey, navigate_callback, param1, param2, param3);
}

/***************************************************************************

 ***************************************************************************/
PRIVATE int navigate_callback(
    const char *path,
    json_t *kw,
    void *param1,
    void *param2,
    void *param3
)
{
    json_t *jn_propagated_keys = param1;
    const char *key = param2;

    if(kw_has_key(kw, key)) {
        char *fullpath = str_concat3(path, delimiter, key);
        json_object_set(jn_propagated_keys, fullpath, json_object_get(kw, key));
        str_concat_free(fullpath);
    }

    return 0;
}

/***************************************************************************
    Return a new kw with values of propagated `keys`
    being the returned keys the full paths of `keys` instances.
    Arrays of dicts are identify by the 'id' field.
 ***************************************************************************/
PUBLIC json_t *kw_get_propagated_key_values(
    json_t *kw,         // not owned
    const char *pkey,
    const char **keys
)
{
    json_t *jn_propagated_keys = json_object();
    while(*keys) {
        kw_navigate(kw, pkey, navigate_callback, jn_propagated_keys, (void *)(*keys), 0);
        keys++;
    }
    return jn_propagated_keys;
}

/***************************************************************************
    Restore values of propagated `keys`
 ***************************************************************************/
PUBLIC int kw_put_propagated_key_values(
    json_t *kw,         // not owned
    const char *pkey,
    json_t *propagated_keys // owned
)
{
    const char *key;
    json_t *jn_value;
    json_object_foreach(propagated_keys, key, jn_value) {
        json_incref(jn_value);
        kw_set_key_value(kw, pkey, key, jn_value);
    }

    json_decref(propagated_keys);
    return 0;
}

/***************************************************************************
 *  Like kw_set_dict_value but managing arrays of dicts with `pkey` (id) field
 ***************************************************************************/
PUBLIC int kw_set_key_value(
    json_t *kw,
    const char *pkey,
    const char *path,   // The last word after . is the key
    json_t *value) // owned
{
    if(!(json_is_object(kw) || json_is_array(kw))) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json object or array",
            "path",         "%s", path,
            NULL
        );
        JSON_DECREF(value);
        return -1;
    }

    char *p = search_delimiter(path, delimiter[0]);
    if(p) {
        char segment[1024];
        if(snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path)>=sizeof(segment)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "buffer too small",
                "path",         "%s", path,
                NULL
            );
        }

        if(empty_string(segment)) {
            return kw_set_key_value(kw, pkey, p+1, value);
        }

        if(json_is_array(kw)) {
            json_t *node_dict = kw_get_record(kw, pkey, json_string(segment));
            if(node_dict) {
                return kw_set_key_value(node_dict, pkey, p+1, value);
            } else {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "kw record not found",
                    "path",         "%s", path,
                    "pkey",         "%s", segment,
                    NULL
                );
                JSON_DECREF(value);
                return -1;
            }
        } else {
            json_t *node_dict = json_object_get(kw, segment);
            if(node_dict) {
                return kw_set_key_value(node_dict, pkey, p+1, value);
            } else {
                // Must exist the path
                return 0;
                //node_dict = json_object();
                //json_object_set_new(kw, segment, node_dict);
                //return kw_set_key_value(node_dict, pkey, p+1, value);
            }
        }
    }

    if(json_is_object(kw)) {
        return json_object_set_new(kw, path, value);
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json object",
            "path",         "%s", path,
            NULL
        );
        JSON_DECREF(value);
        return -1;
    }
}

/***************************************************************************
    Get the dict of dict's array with id
 ***************************************************************************/
json_t *kw_get_record(
    json_t *dict_list, // not owned
    const char *pkey,
    json_t *pkey_value  // owned
)
{
    size_t idx;
    json_t *jn_dict;
    json_array_foreach(dict_list, idx, jn_dict) {
        json_t *jn_id = json_object_get(jn_dict, pkey);
        if(cmp_two_simple_json(jn_id, pkey_value)==0) {
            json_decref(pkey_value);
            return jn_dict;
        }
    }
    json_decref(pkey_value);
    return 0;
}

/***************************************************************************
    Insert dict in dict's array ordered by field pkey
 ***************************************************************************/
int kw_insert_ordered_record(
    json_t *dict_list, // not owned
    const char *pkey,
    json_t *record  // owned
)
{
    json_t *tm = kw_get_dict_value(record, pkey, 0, KW_REQUIRED);
    json_int_t last_instance = json_array_size(dict_list);
    json_int_t idx = last_instance;
    while(idx > 0) {
        json_t *record_ = json_array_get(dict_list, idx-1);
        json_t *tm_ = kw_get_dict_value(record_, pkey, 0, KW_REQUIRED);
        if(cmp_two_simple_json(tm, tm_) >= 0) {
            break;
        }
        idx--;
    }
    json_array_insert_new(dict_list, idx, record);

    return 0;
}

/***************************************************************************
 *  ATTR:
    HACK el eslabón perdido.
    Return a new kw applying __json_config_variables__
 ***************************************************************************/
PUBLIC json_t *kw_apply_json_config_variables(
    json_t *kw,          // not owned
    json_t *jn_global    // not owned
)
{
    char *kw_ = json2str(kw);
    char *jn_global_ = json2str(jn_global);
    char *config = json_config(
        0,
        0,
        0,              // fixed_config
        kw_,            // variable_config
        0,              // config_json_file
        jn_global_,     // parameter_config
        PEF_CONTINUE
    );
    jsonp_free(kw_);
    jsonp_free(jn_global_);
    if(!config) {
        return 0;
    }
    json_t *kw_new = legalstring2json(config, TRUE);
    jsonp_free(config);

    if(!kw_new) {
        return 0;
    }
    return kw_new;
}

/***************************************************************************
    Remove from kw1 all keys in kw2
    kw2 can be a string, dict, or list.
 ***************************************************************************/
PUBLIC int kw_pop(
    json_t *kw1, // not owned
    json_t *kw2  // not owned
)
{
    if(json_is_object(kw2)) {
        const char *key;
        json_t *jn_value;
        json_object_foreach(kw2, key, jn_value) {
            json_object_del(kw1, key);
        }
    } else if(json_is_array(kw2)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw2, idx, jn_value) {
            kw_pop(kw1, jn_value);
        }
    } else if(json_is_string(kw2)) {
        json_object_del(kw1, json_string_value(kw2));
    }
    return 0;
}

/***************************************************************************
    Has word? Can be in string, list or dict.
    options: "recursive", "verbose"

    Use to configurate:

        "opt2"              No has word "opt1"
        "opt1|opt2"         Yes, has word "opt1"
        ["opt1", "opt2"]    Yes, has word "opt1"
        {
            "opt1": true,   Yes, has word "opt1"
            "opt2": false   No has word "opt1"
        }

 ***************************************************************************/
PUBLIC BOOL kw_has_word(
    json_t *kw,  // NOT owned
    const char *word,
    const char *options
)
{
    if(!kw) {
        if(options && strstr(options, "verbose")) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "kw_has_word() kw NULL",
                NULL
            );
        }
        return FALSE;
    }

    switch(json_typeof(kw)) {
    case JSON_OBJECT:
        if(kw_has_key(kw, word)) {
            return json_is_true(json_object_get(kw, word))?TRUE:FALSE;
        } else {
            return FALSE;
        }
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(kw, idx, jn_value) {
                if(json_is_string(jn_value)) {
                    if(strstr(json_string_value(jn_value), word)) {
                        return TRUE;
                    }
                } else if(options && strstr(options, "recursive")) {
                    if(kw_has_word(jn_value, word, options)) {
                        return TRUE;
                    }
                }
            }
            return FALSE;
        }
    case JSON_STRING:
        if(strstr(json_string_value(kw), word)) {
            return TRUE;
        } else {
            return FALSE;
        }
    default:
        if(options && strstr(options, "verbose")) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Searching word needs object,array,string",
                "word",         "%s", word,
                NULL
            );
            log_debug_json(0, kw, "Searching word needs object,array,string");
        }
        return FALSE;
    }
}

/***************************************************************************
    Utility for databases.
    Get a json item walking by the tree (routed by path)
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter are '`' and '.'
 ***************************************************************************/
PRIVATE json_t *_kwid_get(
    const char *options, // "verbose", "lower", "backward"
    json_t *kw,  // NOT owned
    char *path
)
{
    BOOL verbose = (options && strstr(options, "verbose"))?TRUE:FALSE;
    BOOL backward = (options && strstr(options, "backward"))?TRUE:FALSE;

    if(options && strstr(options, "lower")) {
        strtolower(path);
    }

    int list_size;
    const char **segments = split2(path, "`.", &list_size);

    json_t *v = kw;
    BOOL fin = FALSE;
    for(int i=0; i<list_size && !fin; i++) {
        const char *segment = *(segments +i);

        if(!v) {
            if(verbose) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "short path",
                    "path",         "%s", path,
                    "segment",      "%s", segment,
                    NULL
                );
            }
            break;
        }

        switch(json_typeof(v)) {
        case JSON_OBJECT:
            v = json_object_get(v, segment);
            if(!v) {
                fin = TRUE;
            }
            break;
        case JSON_ARRAY:
            {
                int idx; json_t *v_;
                BOOL found = FALSE;
                if(!backward) {
                    json_array_foreach(v, idx, v_) {
                        const char *id = json_string_value(json_object_get(v_, "id"));
                        if(id && strcmp(id, segment)==0) {
                            v = v_;
                            found = TRUE;
                            break;
                        }
                    }
                    if(!found) {
                        v = 0;
                        fin = TRUE;
                    }
                } else {
                    json_array_backward(v, idx, v_) {
                        const char *id = json_string_value(json_object_get(v_, "id"));
                        if(id && strcmp(id, segment)==0) {
                            v = v_;
                            found = TRUE;
                            break;
                        }
                    }
                    if(!found) {
                        v = 0;
                        fin = TRUE;
                    }
                }

            }
            break;
        default:
            fin = TRUE;
            break;
        }
    }

    split_free2(segments);

    return v;
}

/***************************************************************************
    Utility for databases.
    Get a json item walking by the tree (routed by path)
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter are '`' and '.'
 ***************************************************************************/
PUBLIC json_t *kwid_get( // Return is NOT YOURS
    const char *options, // "verbose", "lower", "backward"
    json_t *kw,  // NOT owned
    const char *path,
    ...
)
{
    va_list ap;
    char temp[4*1024]; temp[0] = 0;

    va_start(ap, path);
    vsnprintf(
        temp,
        sizeof(temp),
        path,
        ap
    );

    json_t *jn = _kwid_get(options, kw, temp);

    va_end(ap);

    return jn;
}

/***************************************************************************
    Utility for databases.
    Return a new list from a "dict of records" or "list of records"
    WARNING the "id" of a dict's record is hardcorded to his key.
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'

    If path is empty then use kw
 ***************************************************************************/
PUBLIC json_t *kwid_new_list(
    const char *options, // "verbose", "lower"
    json_t *kw,  // NOT owned
    const char *path,
    ...
)
{
    va_list ap;
    char temp[4*1024]; temp[0] = 0;

    va_start(ap, path);
    vsnprintf(
        temp,
        sizeof(temp),
        path,
        ap
    );

    json_t *jn = _kwid_get(options, kw, temp);

    va_end(ap);

    if(!jn) {
        // Error already logged if verbose
        return 0;
    }
    json_t *new_list = 0;

    switch(json_typeof(jn)) {
    case JSON_ARRAY:
        {
            new_list = jn;
            json_incref(new_list);
        }
        break;
    case JSON_OBJECT:
        {
            new_list = json_array();
            const char *key; json_t *v;
            json_object_foreach(jn, key, v) {
                json_object_set_new(v, "id", json_string(key)); // WARNING id hardcorded
                json_array_append(new_list, v);
            }
        }
        break;
    default:
        if(options && strstr(options, "verbose")) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "wrong type for list",
                "path",         "%s", temp,
                "jn",           "%j", jn,
                NULL
            );
        }
        break;
    }

    return new_list;
}

/***************************************************************************
    Utility for databases.
    Return a new dict from a "dict of records" or "list of records"
    WARNING the "id" of a dict's record is hardcorded to his key.
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'
    If path is empty then use kw
 ***************************************************************************/
PUBLIC json_t *kwid_new_dict(
    const char *options, // "verbose", "lower"
    json_t *kw,  // NOT owned
    const char *path,
    ...
)
{
    va_list ap;
    char temp[4*1024]; temp[0] = 0;

    va_start(ap, path);
    vsnprintf(
        temp,
        sizeof(temp),
        path,
        ap
    );

    json_t *jn = _kwid_get(options, kw, temp);

    va_end(ap);

    if(!jn) {
        // Error already logged if verbose
        return 0;
    }

    json_t *new_dict = 0;

    switch(json_typeof(jn)) {
    case JSON_ARRAY:
        {
            new_dict = json_object();
            int idx; json_t *v;
            json_array_foreach(jn, idx, v) {
                const char *id = kw_get_str(v, "id", "", KW_REQUIRED);
                if(!empty_string(id)) {
                    json_object_set(new_dict, id, v);
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            new_dict = jn;
            json_incref(new_dict);
        }
        break;
    default:
        if(options && strstr(options, "verbose")) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "wrong type for dict",
                "path",         "%s", temp,
                "jn",           "%j", jn,
                NULL
            );
        }
        break;
    }

    return new_dict;
}

/***************************************************************************
    Utility for databases.
    Return TRUE if `id` is in the list/dict/str `ids`
 ***************************************************************************/
PUBLIC BOOL kwid_match_id(json_t *ids, const char *id)
{
    if(!ids || !id) {
        // Si no hay filtro pasan todos.
        return TRUE;
    }
    if(json_is_object(ids) && json_object_size(ids)==0) {
        // A empty object at first level evaluate as true.
        return TRUE;
    }
    if(json_is_array(ids) && json_array_size(ids)==0) {
        // A empty object at first level evaluate as true.
        return TRUE;
    }

    switch(json_typeof(ids)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(ids, idx, jn_value) {
                if(json_is_string(jn_value)) {
                    const char *value = json_string_value(jn_value);
                    if(value && strcmp(id, value)==0)  {
                        return TRUE;
                    }
                } else if(json_is_object(jn_value)) {
                    const char *value = kw_get_str(jn_value, "id", 0, 0);
                    if(value && strcmp(id, value)==0)  {
                        return TRUE;
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *key; json_t *jn_value;
            json_object_foreach(ids, key, jn_value) {
                if(strcmp(id, key)==0)  {
                    return TRUE;
                }
            }
        }
        break;

    case JSON_STRING:
        if(strcmp(id, json_string_value(ids))==0) {
            return TRUE;
        }
        break;

    default:
        break;
    }
    return FALSE;
}

/***************************************************************************
    Utility for databases.
    Return TRUE if `id` WITH LIMITED SIZE is in the list/dict/str `ids`
 ***************************************************************************/
PUBLIC BOOL kwid_match_nid(json_t *ids, const char *id, int max_id_size)
{
    if(!ids || !id) {
        // Si no hay filtro pasan todos.
        return TRUE;
    }
    if(json_is_object(ids) && json_object_size(ids)==0) {
        // A empty object at first level evaluate as true.
        return TRUE;
    }
    if(json_is_array(ids) && json_array_size(ids)==0) {
        // A empty object at first level evaluate as true.
        return TRUE;
    }

    switch(json_typeof(ids)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(ids, idx, jn_value) {
                if(json_is_string(jn_value)) {
                    const char *value = json_string_value(jn_value);
                    if(value && strncmp(id, value, max_id_size)==0)  {
                        return TRUE;
                    }
                } else if(json_is_object(jn_value)) {
                    const char *value = kw_get_str(jn_value, "id", 0, 0);
                    if(value && strncmp(id, value, max_id_size)==0)  {
                        return TRUE;
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *key; json_t *jn_value;
            json_object_foreach(ids, key, jn_value) {
                if(strncmp(id, key, max_id_size)==0)  {
                    return TRUE;
                }
            }
        }
        break;

    case JSON_STRING:
        if(strncmp(id, json_string_value(ids), max_id_size)==0) {
            return TRUE;
        }
        break;

    default:
        break;
    }
    return FALSE;
}

/***************************************************************************
    Utility for databases.
    Being `kw` a:
        - list of strings [s,...]
        - list of dicts [{},...]
        - dict of dicts {id:{},...}
    return a **NEW** list of incref (clone) kw filtering the rows by `jn_filter` (where),
    and matching the ids.
    If match_fn is 0 then kw_match_simple is used.
    NOTE Using JSON_INCREF/JSON_DECREF HACK
 ***************************************************************************/
PUBLIC json_t *kwid_collect( // WARNING be care, you can modify the original records
    json_t *kw,         // not owned
    json_t *ids,        // owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    if(!kw) {
        JSON_DECREF(ids);
        JSON_DECREF(jn_filter);
        // silence
        return 0;
    }
    if(!match_fn) {
        match_fn = kw_match_simple;
    }

    json_t *kw_new = json_array();

    if(json_is_array(kw)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw, idx, jn_value) {
            const char *id;
            if(json_is_object(jn_value)) {
                id = kw_get_str(jn_value, "id", 0, 0);
            } else if(json_is_string(jn_value)) {
                id = json_string_value(jn_value);
            } else {
                continue;
            }

            if(!kwid_match_id(ids, id)) {
                continue;
            }
            JSON_INCREF(jn_filter);
            if(match_fn(jn_value, jn_filter)) {
                json_array_append(kw_new, jn_value);
            }
        }
    } else if(json_is_object(kw)) {
        const char *id; json_t *jn_value;
        json_object_foreach(kw, id, jn_value) {
            if(!kwid_match_id(ids, id)) {
                continue;
            }
            JSON_INCREF(jn_filter);
            if(match_fn(jn_value, jn_filter)) {
                json_array_append(kw_new, jn_value);
            }
        }

    } else  {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array or object",
            NULL
        );
        JSON_DECREF(kw_new);
    }

    JSON_DECREF(ids);
    JSON_DECREF(jn_filter);
    return kw_new;
}

/***************************************************************************
    Utility for databases.
    Being `ids` a:

        "$id"

        {
            "$id": {
                "id": "$id",
                ...
            }
            ...
        }

        ["$id", ...]

        [
            "$id",
            {
                "id":$id,
                ...
            },
            ...
        ]

    return a list of all ids (all duplicated items)
 ***************************************************************************/
PUBLIC json_t *kwid_get_ids(
    json_t *ids // not owned
)
{
    if(!ids) {
        return 0;
    }

    json_t *new_ids = json_array();

    switch(json_typeof(ids)) {
    case JSON_STRING:
        /*
            "$id"
         */
        json_array_append_new(new_ids, json_string(json_string_value(ids)));
        break;

    case JSON_INTEGER:
        /*
            $id
         */
        json_array_append_new(
            new_ids,
            json_sprintf("%"JSON_INTEGER_FORMAT, json_integer_value(ids))
        );
        break;

    case JSON_OBJECT:
        /*
            {
                "$id": {
                    "id": "$id",
                    ...
                }
                ...
            }
        */
        {
            const char *id; json_t *jn_value;
            json_object_foreach(ids, id, jn_value) {
                json_array_append_new(new_ids, json_string(id));
            }
        }
        break;

    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(ids, idx, jn_value) {
                switch(json_typeof(jn_value)) {
                case JSON_STRING:
                    /*
                        ["$id", ...]
                    */
                    {
                        const char *id = json_string_value(jn_value);
                        if(!empty_string(id)) {
                            json_array_append_new(new_ids, json_string(id));
                        }
                    }
                    break;

                case JSON_INTEGER:
                    /*
                        $id
                    */
                    json_array_append_new(
                        new_ids,
                        json_sprintf("%"JSON_INTEGER_FORMAT, json_integer_value(jn_value))
                    );
                    break;

                case JSON_OBJECT:
                    /*
                        [
                            {
                                "id":$id,
                                ...
                            },
                            ...
                        ]
                    */
                    {
                        const char *id = json_string_value(json_object_get(jn_value, "id"));
                        if(!empty_string(id)) {
                            json_array_append_new(new_ids, json_string(id));
                        }
                    }
                    break;
                default:
                    break;
                }
            }
        }
        break;

    default:
        break;
    }

    return new_ids;
}

/***************************************************************************
    Utility for databases.
    Return a list with id-records:

        [{"id": "$id", ...}, ...]

        {
            "$id": {"id": "$id",...},
            ...
        }

        {
            "hook": [{"id": "$id", ...}, ...]
            ...
        }


    array-object:

        [
            {
                "id": "$id",
                ...
            },
            ...
        ]

    object-object:

        {
            "$id": {
                "id": "$id",
                ...
            }
            ...
        }

    object-array-object:

        {
            "hook" = [
                {
                    "id": "$id",
                    ...
                },
                ...
            ]
        }

 ***************************************************************************/
PUBLIC json_t *kwid_get_id_records(
    json_t *records // not owned
)
{
    if(!records) {
        return 0;
    }

    json_t *new_id_record_list = json_array();

    switch(json_typeof(records)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(records, idx, jn_value) {
                switch(json_typeof(jn_value)) {
                case JSON_OBJECT:
                    /*
                        [
                            {
                                "id": "$id",
                                ...
                            },
                            ...
                        ]
                    */
                    {
                        const char *id = json_string_value(json_object_get(jn_value, "id"));
                        if(!empty_string(id)) {
                            json_array_append(new_id_record_list, jn_value);
                        }
                    }
                    break;
                default:
                    break;
                }
            }
        }
        break;

    case JSON_OBJECT:
        {
            const char *id; json_t *jn_value;
            json_object_foreach(records, id, jn_value) {
                switch(json_typeof(jn_value)) {
                case JSON_OBJECT:
                    {
                        /*
                            {
                                "$id": {
                                    "id": "$id",
                                    ...
                                }
                                ...
                            }
                        */
                        const char *id_ = json_string_value(json_object_get(jn_value, "id"));
                        if(id_ && strcmp(id_, id)==0) {
                            json_array_append(new_id_record_list, jn_value);
                        }
                    }
                    break;
                case JSON_ARRAY:
                    {
                        int idx; json_t *jn_r;
                        json_array_foreach(jn_value, idx, jn_r) {
                            switch(json_typeof(jn_r)) {
                            case JSON_OBJECT:
                                /*
                                    "hook" = [
                                        {
                                            "id": "$id",
                                            ...
                                        },
                                        ...
                                    ]
                                */
                                {
                                    const char *id =
                                        json_string_value(json_object_get(jn_r, "id"));
                                    if(!empty_string(id)) {
                                        json_array_append(new_id_record_list, jn_r);
                                    }
                                }
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    break;
                default:
                    break;
                }
            }
        }
        break;

    default:
        break;
    }

    return new_id_record_list;
}

/***************************************************************************
    Utility for databases.
    Being field `kw` a list of id record [{id...},...] return the record idx with `id`
    Return -1 if not found
 ***************************************************************************/
int kwid_find_record_in_list(
    const char *options, // "verbose"
    json_t *kw_list,
    const char *id
)
{
    if(!id || !json_is_array(kw_list)) {
        if(options && strstr(options, "verbose")) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "id NULL or kw_list is not a list",
                NULL
            );
        }
        return -1;
    }

    int idx; json_t *record;
    json_array_foreach(kw_list, idx, record) {
        const char *id_ = kw_get_str(record, "id", 0, 0);
        if(!id_) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "list item is not a id record",
                NULL
            );
            return -1;
        }
        if(strcmp(id, id_)==0) {
            return idx;
        }
    }

    if(options && strstr(options, "verbose")) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "record not found in this list",
            "id",           "%s", id,
            NULL
        );
    }
    return 0;
}

/***************************************************************************
    Get a the idx of simple json item in a json list.
    Return -1 if not found
 ***************************************************************************/
PUBLIC int kw_find_json_in_list(
    const char *options, // "verbose"
    json_t *kw_list,
    json_t *item
)
{
    if(!item || !json_is_array(kw_list)) {
        if(options && strstr(options, "verbose")) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "item NULL or kw_list is not a list",
                NULL
            );
        }
        return -1;
    }

    int idx;
    json_t *jn_item;

    json_array_foreach(kw_list, idx, jn_item) {
        if(kw_is_identical(item, jn_item)) {
            return idx;
        }
    }

    if(options && strstr(options, "verbose")) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "item not found in this list",
            "item",         "%j", item,
            NULL
        );
    }
    return -1;
}

/***************************************************************************
    Get a the idx of simple json item in a json list.
    Return -1 if not found
 ***************************************************************************/
PUBLIC int kw_find_str_in_list(
    json_t *kw_list,
    const char *str
)
{
    if(!str || !json_is_array(kw_list)) {
        return -1;
    }

    int idx;
    json_t *jn_str;
    json_array_foreach(kw_list, idx, jn_str) {
        const char *str_ = json_string_value(jn_str);
        if(!str_) {
            continue;
        }
        if(strcmp(str, str_)==0) {
            return idx;
        }
    }

    return -1;
}

/***************************************************************************
    Compare deeply two json **records**. Can be disordered.
 ***************************************************************************/
PUBLIC BOOL kwid_compare_records(
    json_t *record_, // NOT owned
    json_t *expected_, // NOT owned
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
)
{
    BOOL ret = TRUE;
    json_t *record = json_deep_copy(record_);
    json_t *expected = json_deep_copy(expected_);
    if(!record) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "record NULL",
                NULL
            );
        }
        JSON_DECREF(record);
        JSON_DECREF(expected);
        return FALSE;
    }
    if(!expected) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "expected NULL",
                NULL
            );
        }
        JSON_DECREF(record);
        JSON_DECREF(expected);
        return FALSE;
    }

    if(json_typeof(record) != json_typeof(expected)) { // json_typeof CONTROLADO
        ret = FALSE;
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "different json type",
                "record",       "%j", record,
                "expected",     "%j", expected,
                NULL
            );
        }
    } else {
        switch(json_typeof(record)) {
            case JSON_ARRAY:
                {
                    if(!kwid_compare_lists(
                            record,
                            expected,
                            without_metadata,
                            without_private,
                            verbose)) {
                        ret = FALSE;
                        if(verbose) {
                            log_error(0,
                                "gobj",         "%s", __FILE__,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "list not match",
                                "record",       "%j", record,
                                "expected",     "%j", expected,
                                NULL
                            );
                        }
                    }
                }
                break;

            case JSON_OBJECT:
                {
                    if(without_metadata) {
                        kw_delete_metadata_keys(record);
                        kw_delete_metadata_keys(expected);
                    }
                    if(without_private) {
                        kw_delete_private_keys(record);
                        kw_delete_private_keys(expected);
                    }

                    void *n; const char *key; json_t *value;
                    json_object_foreach_safe(record, n, key, value) {
                        if(!kw_has_key(expected, key)) {
                            ret = FALSE;
                            if(verbose) {
                                log_error(0,
                                    "gobj",         "%s", __FILE__,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "key not found",
                                    "key",          "%s", key,
                                    "expected",     "%j", expected,
                                    NULL
                                );
                            }
                            break;
                        }
                        json_t *value2 = json_object_get(expected, key);
                        if(json_typeof(value)==JSON_OBJECT) {
                            if(!kwid_compare_records(
                                    value,
                                    value2,
                                    without_metadata,
                                    without_private,
                                    verbose
                                )) {
                                ret = FALSE;
                                if(verbose) {
                                    log_error(0,
                                        "gobj",         "%s", __FILE__,
                                        "function",     "%s", __FUNCTION__,
                                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                        "msg",          "%s", "record not match",
                                        "record",       "%j", record,
                                        "expected",     "%j", expected,
                                        NULL
                                    );
                                }
                            }
                            if(ret == FALSE) {
                                break;
                            }

                            json_object_del(record, key);
                            json_object_del(expected, key);

                        } else if(json_typeof(value)==JSON_ARRAY) {
                            if(!kwid_compare_lists(
                                    value,
                                    value2,
                                    without_metadata,
                                    without_private,
                                    verbose
                                )) {
                                ret = FALSE;
                                if(verbose) {
                                    log_error(0,
                                        "gobj",         "%s", __FILE__,
                                        "function",     "%s", __FUNCTION__,
                                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                        "msg",          "%s", "list not match",
                                        "record",       "%j", record,
                                        "expected",     "%j", expected,
                                        NULL
                                    );
                                }
                            }
                            if(ret == FALSE) {
                                break;
                            }

                            json_object_del(record, key);
                            json_object_del(expected, key);

                        } else {
                            if(cmp_two_simple_json(value, value2)!=0) {
                                ret = FALSE;
                                if(verbose) {
                                    log_error(0,
                                        "gobj",         "%s", __FILE__,
                                        "function",     "%s", __FUNCTION__,
                                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                        "msg",          "%s", "items not match",
                                        "value",        "%j", value,
                                        "value2",       "%j", value2,
                                        NULL
                                    );
                                }
                                break;
                            } else {
                                json_object_del(record, key);
                                json_object_del(expected, key);
                            }
                        }
                    }

                    if(ret == TRUE) {
                        if(json_object_size(record)>0) {
                            ret = FALSE;
                            if(verbose) {
                                log_error(0,
                                    "gobj",         "%s", __FILE__,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "remain record items",
                                    "record",       "%j", record,
                                    NULL
                                );
                            }
                        }
                        if(json_object_size(expected)>0) {
                            ret = FALSE;
                            if(verbose) {
                                log_error(0,
                                    "gobj",         "%s", __FILE__,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "remain expected items",
                                    "expected",     "%j", expected,
                                    NULL
                                );
                            }
                        }
                    }
                }
                break;
            default:
                ret = FALSE;
                if(verbose) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "No list or not object",
                        "record",       "%j", record,
                        NULL
                    );
                }
                break;
        }
    }

    JSON_DECREF(record);
    JSON_DECREF(expected);
    return ret;
}

/***************************************************************************
    Compare deeply two json lists of **records**. Can be disordered.
 ***************************************************************************/
PUBLIC BOOL kwid_compare_lists(
    json_t *list_, // NOT owned
    json_t *expected_, // NOT owned
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
)
{
    BOOL ret = TRUE;
    json_t *list = json_deep_copy(list_);
    json_t *expected = json_deep_copy(expected_);
    if(!list) {
        JSON_DECREF(list);
        JSON_DECREF(expected);
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "record NULL",
                NULL
            );
        }
        return FALSE;
    }
    if(!expected) {
        JSON_DECREF(list);
        JSON_DECREF(expected);
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "expected NULL",
                NULL
            );
        }
        return FALSE;
    }

    if(json_typeof(list) != json_typeof(expected)) { // json_typeof CONTROLADO
        ret = FALSE;
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "different json type",
                "list",         "%j", list,
                "expected",     "%j", expected,
                NULL
            );
        }
    } else {
        switch(json_typeof(list)) {
        case JSON_ARRAY:
            {
                int idx1; json_t *r1;
                json_array_foreach(list, idx1, r1) {
                    const char *id1 = kw_get_str(r1, "id", 0, 0);
                    /*--------------------------------*
                     *  List with id records
                     *--------------------------------*/
                    if(id1) {
                        size_t idx2 = kwid_find_record_in_list("", expected, id1);
                        if(idx2 < 0) {
                            ret = FALSE;
                            if(verbose) {
                                log_error(0,
                                    "gobj",         "%s", __FILE__,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "record not found in expected list",
                                    "record",       "%j", r1,
                                    "expected",     "%j", expected,
                                    NULL
                                );
                            }
                            continue;
                        }
                        json_t *r2 = json_array_get(expected, idx2);

                        if(!kwid_compare_records(
                            r1,
                            r2,
                            without_metadata,
                            without_private,
                            verbose)
                        ) {
                            ret = FALSE;
                            if(verbose) {
                                log_error(0,
                                    "gobj",         "%s", __FILE__,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "record not match",
                                    "r1",           "%j", r1,
                                    "r2",           "%j", r2,
                                    NULL
                                );
                            }
                        }
                        if(ret == FALSE) {
                            break;
                        }

                        if(json_array_remove(list, idx1)==0) {
                            idx1--;
                        }
                        json_array_remove(expected, idx2);
                    } else {
                        /*--------------------------------*
                         *  List with any json items
                         *--------------------------------*/
                        int idx2 = kw_find_json_in_list("", expected, r1);
                        if(idx2 < 0) {
                            ret = FALSE;
                            if(verbose) {
                                log_error(0,
                                    "gobj",         "%s", __FILE__,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "record not found in expected list",
                                    "record",       "%j", r1,
                                    "expected",     "%j", expected,
                                    NULL
                                );
                            }
                            break;
                        }
                        if(json_array_remove(list, idx1)==0) {
                            idx1--;
                        }
                        json_array_remove(expected, idx2);
                    }
                }

                if(ret == TRUE) {
                    if(json_array_size(list)>0) {
                        if(verbose) {
                            log_error(0,
                                "gobj",         "%s", __FILE__,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "remain list items",
                                "list",         "%j", list,
                                NULL
                            );
                        }
                        ret = FALSE;
                    }
                    if(json_array_size(expected)>0) {
                        if(verbose) {
                            log_error(0,
                                "gobj",         "%s", __FILE__,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "remain expected items",
                                "expected",     "%j", expected,
                                NULL
                            );
                        }
                        ret = FALSE;
                    }
                }
            }
            break;

        case JSON_OBJECT:
            {
                if(!kwid_compare_records(
                    list,
                    expected,
                    without_metadata,
                    without_private,
                    verbose)
                ) {
                    ret = FALSE;
                    if(verbose) {
                        trace_msg("ERROR: object not match");
                    }
                }
            }
            break;
        default:
            {
                ret = FALSE;
                if(verbose) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "No list or not object",
                        "list",         "%j", list,
                        NULL
                    );
                }
            }
            break;
        }
    }

    JSON_DECREF(list);
    JSON_DECREF(expected);
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _tree_select(
    json_t *new_id_record_list,
    json_t *kw,         // not owned
    json_t *jn_fields,  // not owned
    const char *tree_hook,
    json_t *jn_filter,  // not owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    ),
    BOOL verbose
)
{
    if(!kw) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw NULL",
            NULL
        );
        return -1;
    }

    switch(json_typeof(kw)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(kw, idx, jn_value) {
                switch(json_typeof(jn_value)) {
                case JSON_OBJECT:
                    /*
                        array-object:

                            [{"id": "$id", ...}, ...]
                    */
                    _tree_select(
                        new_id_record_list,
                        jn_value,    // not owned
                        jn_fields,   // not owned
                        tree_hook,
                        jn_filter,  // not owned
                        match_fn,
                        verbose
                    );
                    break;
                default:
                    break;
                }
            }
        }
        break;

    case JSON_OBJECT:
        {
            const char *key; json_t *jn_value;
            const char *id = kw_get_str(kw, "id", 0, 0); // Is it a id-record?
            if(!id) {
                /*
                    No a id-record, must be a dict of records

                    object-object:

                        {
                            "$id": {"id": "$id",...},
                            ...
                        }
                */
                json_object_foreach(kw, key, jn_value) {
                    const char *key_ = kw_get_str(jn_value, "id", 0, 0);
                    if(key_ && strcmp(key_, key)==0) {
                        _tree_select(
                            new_id_record_list,
                            jn_value,    // not owned
                            jn_fields,   // not owned
                            tree_hook,
                            jn_filter,  // not owned
                            match_fn,
                            verbose
                        );
                    }
                }

            } else {
                /*
                 *  It's a id-record, select fields and walk over hook
                 */
                JSON_INCREF(jn_filter);
                if(match_fn(kw, jn_filter)) {
                    JSON_INCREF(kw);
                    JSON_INCREF(jn_fields);
                    json_t *jn_select = kw_clone_by_keys(kw, jn_fields, verbose);
                    if(jn_select && json_object_size(jn_select)>0) {
                        json_array_append_new(new_id_record_list, jn_select);
                    } else {
                        JSON_DECREF(jn_select);
                    }
                }

                json_object_foreach(kw, key, jn_value) {
                    if(strcmp(key, tree_hook)==0) {
                        // Walk the tree, hook field
                        _tree_select(
                            new_id_record_list,
                            jn_value,    // not owned
                            jn_fields,   // not owned
                            tree_hook,
                            jn_filter,  // not owned
                            match_fn,
                            verbose
                        );
                    }
                }
            }
        }
        break;

    default:
        break;
    }

    return 0;
}

/***************************************************************************
    Utility for databases.

    Being `kw` a:

    array-object:

        [{"id": "$id", ...}, ...]

    object-object:

        {
            "$id": {"id": "$id",...},
            ...
        }

    object-array-object:

        {
            "hook": [{"id": "$id", ...}, ...]
            ...
        }

    return a **NEW** list of dicts with only `fields` keys,
    filtering the rows by `jn_filter` (where),
    and walking the tree through `tree_hook` key.

    jn_fields can be
        "$field"
        ["$field1", "$field2",...]
        {"$field1":{}, "$field2":{},...}

    If match_fn is 0 then kw_match_simple is used.
    NOTE Using JSON_INCREF/JSON_DECREF HACK
 ***************************************************************************/
PUBLIC json_t *kwid_tree_select( // WARNING be care, you can modify the origina kw
    json_t *kw,             // not owned
    json_t *jn_fields,      // owned
    const char *tree_hook,
    json_t *jn_filter,      // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    ),
    BOOL verbose
)
{
    if(!kw) {
        JSON_DECREF(jn_fields);
        JSON_DECREF(jn_filter);
        // silence
        return 0;
    }
    if(!match_fn) {
        match_fn = kw_match_simple;
    }

    json_t *new_id_record_list = json_array();
    _tree_select(
        new_id_record_list,
        kw,
        jn_fields,
        tree_hook,
        jn_filter,
        match_fn,
        verbose
    );
    JSON_DECREF(jn_fields);
    JSON_DECREF(jn_filter);
    return new_id_record_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _tree_collect(
    json_t *new_dict_or_list,
    json_t *kw,         // not owned
    json_t *jn_filter,  // not owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    if(!kw) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw NULL",
            NULL
        );
        return -1;
    }

    switch(json_typeof(kw)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(kw, idx, jn_value) {
                switch(json_typeof(jn_value)) {
                case JSON_OBJECT:
                    {
                        _tree_collect(
                            new_dict_or_list,
                            jn_value,   // not owned
                            jn_filter,  // not owned
                            match_fn
                        );
                    }
                    break;
                default:
                    break;
                }
            }
        }
        break;

    case JSON_OBJECT:
        {
            const char *id = kw_get_str(kw, "id", 0, 0); // Is it a id-record?
            if(id) {
                /*
                 *  It's a id-record
                 */
                JSON_INCREF(jn_filter);
                if(match_fn(kw, jn_filter)) {
                    if(json_is_object(new_dict_or_list)) {
                        json_object_set(new_dict_or_list, id, kw);
                    } else {
                        json_array_append(new_dict_or_list, kw);
                    }
                }
            }

            /*
             *  Search in more levels
             */
            const char *key; json_t *jn_value;
            json_object_foreach(kw, key, jn_value) {
                if(json_is_array(jn_value) || json_is_object(jn_value)) {
                    _tree_collect(
                        new_dict_or_list,
                        jn_value,   // not owned
                        jn_filter,  // not owned
                        match_fn
                    );
                }
            }
        }
        break;

    default:
        break;
    }

    return 0;
}

/***************************************************************************
    Utility for databases.

    Return a new dict with all id-records found in `kw`
 ***************************************************************************/
PUBLIC json_t *kwid_new_dict_tree_collect( // WARNING be care, you can modify the original kw
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    if(!kw) {
        JSON_DECREF(jn_filter);
        // silence
        return 0;
    }
    if(!match_fn) {
        match_fn = kw_match_simple;
    }
    json_t *new_dict = json_object();

    _tree_collect(
        new_dict,
        kw,
        jn_filter,
        match_fn
    );

    JSON_DECREF(jn_filter);
    return new_dict;
}

/***************************************************************************
    Utility for databases.

    Return a new list with all id-records found in `kw`
 ***************************************************************************/
PUBLIC json_t *kwid_new_list_tree_collect( // WARNING be care, you can modify the original kw
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    if(!kw) {
        JSON_DECREF(jn_filter);
        // silence
        return 0;
    }
    if(!match_fn) {
        match_fn = kw_match_simple;
    }

    json_t *new_list = json_array();

    _tree_collect(
        new_list,
        kw,
        jn_filter,
        match_fn
    );

    JSON_DECREF(jn_filter);
    return new_list;
}

/***************************************************************************
    Return a new json with all arrays or dicts greater than `limit`
        with [{"__collapsed__": {"path": path, "size": size}}]
        or   {{"__collapsed__": {"path": path, "size": size}}}
 ***************************************************************************/
PRIVATE json_t *collapse(
    json_t *kw,         // not owned
    char *path,
    int collapse_lists_limit,
    int collapse_dicts_limit
)
{
    if(!json_is_object(kw)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "collapse() kw must be a dictionary",
            NULL
        );
        return 0;
    }
    json_t *new_kw = json_object();
    const char *key; json_t *jn_value;
    json_object_foreach(kw, key, jn_value) {
        char *new_path = gbmem_strndup(path, strlen(path)+strlen(key)+2);
        if(strlen(new_path)>0) {
            strcat(new_path, delimiter);
        }
        strcat(new_path, key);

        if(json_is_object(jn_value)) {
            if(collapse_dicts_limit>0 && json_object_size(jn_value)>collapse_dicts_limit) {
                json_object_set_new(
                    new_kw,
                    key,
                    json_pack("{s:{s:s, s:I}}",
                        "__collapsed__",
                            "path", new_path,
                            "size", (json_int_t)json_object_size(jn_value)
                    )
                );
            } else {
                json_object_set_new(
                    new_kw,
                    key,
                    collapse(jn_value, new_path, collapse_lists_limit, collapse_dicts_limit)
                );
            }

        } else if(json_is_array(jn_value)) {
            if(collapse_lists_limit > 0 && json_array_size(jn_value)>collapse_lists_limit) {
                json_object_set_new(
                    new_kw,
                    key,
                    json_pack("[{s:{s:s, s:I}}]",
                        "__collapsed__",
                            "path", new_path,
                            "size", (json_int_t)json_array_size(jn_value)
                    )
                );
            } else {
                json_t *new_list = json_array();
                json_object_set_new(new_kw, key, new_list);
                int idx; json_t *v;
                json_array_foreach(jn_value, idx, v) {
                    char s_idx[40];
                    snprintf(s_idx, sizeof(s_idx), "%d", idx);
                    char *new_path2 = gbmem_strndup(new_path, strlen(new_path)+strlen(s_idx)+2);
                    if(strlen(new_path2)>0) {
                        strcat(new_path2, delimiter);
                    }
                    strcat(new_path2, s_idx);

                    if(json_is_object(v)) {
                        json_array_append_new(
                            new_list,
                            collapse(v, new_path2, collapse_lists_limit, collapse_dicts_limit)
                        );
                    } else if(json_is_array(v)) {
                        json_array_append(new_list, v); // ???
                    } else {
                        json_array_append(new_list, v);
                    }
                    gbmem_free(new_path2);
                }
            }

        } else {
            json_object_set(
                new_kw,
                key,
                jn_value
            );
        }
        gbmem_free(new_path);
    }

    return new_kw;
}
PUBLIC json_t *kw_collapse(
    json_t *kw,         // not owned
    int collapse_lists_limit,
    int collapse_dicts_limit
)
{
    if(!json_is_object(kw)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw_collapse() kw must be a dictionary",
            NULL
        );
        return 0;
    }
    char *path = gbmem_malloc(1); *path = 0;
    json_t *new_kw = collapse(kw, path, collapse_lists_limit, collapse_dicts_limit);
    gbmem_free(path);

    return new_kw;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _kwid_walk_childs(
    json_t *parent,  // not owned
    json_t *node,  // not owned
    const char *child_key,
    int (*callback)(
        json_t *parent,  // not owned
        json_t *child,  // not owned
        void *user_data
    ),
    void *user_data
)
{
    int ret = 0;

    /*
     *  Enter in the node
     */
    ret = (*callback)(parent, node, user_data);
    if(ret < 0) {
        return -1;
    }

    /*
     *  Get child list
     */
    json_t *childs = kw_get_list(node, child_key, 0, 0);
    if(!childs) {
        return 0;
    }

    int idx; json_t *child;
    json_array_foreach(childs, idx, child) {
        /*
         *  Entra en los hijos
         */
        ret = _kwid_walk_childs(node, child, child_key, callback, user_data);
        if(ret < 0) {
            break;
        }
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int kwid_walk_childs(
    json_t *kw,  // not owned
    const char *child_key,
    int (*callback)(
        json_t *parent,  // not owned
        json_t *child,  // not owned
        void *user_data
    ),
    void *user_data
)
{
    return _kwid_walk_childs(0, kw, child_key, callback, user_data);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int _dict_content_size(json_t *kw)
{
    int size = 5;

    const char *key; json_t *v;
    json_object_foreach(kw, key, v) {
        size += strlen(key) + 4;
        size += kw_content_size(v);
    }

    return size;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int _list_content_size(json_t *kw)
{
    int size = 5;

    int idx; json_t *v;
    json_array_foreach(kw, idx, v) {
        size += 2;
        size += kw_content_size(v);
    }

    return size;
}

/***************************************************************************
 *  WARNING size for ugly json string (without indent)
 ***************************************************************************/
PUBLIC int kw_content_size(json_t *kw)
{
    int size = 0;

    if(json_is_object(kw)) {
        size = _dict_content_size(kw);

    } else if(json_is_array(kw)) {
        size = _list_content_size(kw);

    } else if(json_is_string(kw)) {
        size = strlen(json_string_value(kw));

    } else if(json_is_boolean(kw) || json_is_null(kw)) {
        size = 8;

    } else {
        size = 64; // for integer, real
    }

    return size;
}

/***************************************************************************
 *  size of dict or size of list, remains return 1
 ***************************************************************************/
PUBLIC int kw_size(json_t *kw)
{
    int size = 0;

    if(json_is_object(kw)) {
        size = json_object_size(kw);

    } else if(json_is_array(kw)) {
        size = json_array_size(kw);

    } else {
        size = 1;
    }

    return size;
}

/***************************************************************************
 *  Metadata key (variable) has a prefix of 2 underscore
 ***************************************************************************/
PUBLIC BOOL is_metadata_key(const char *key)
{
    if(!key) {
        return FALSE;
    }
    int i;
    for(i = 0; i < strlen(key); i++) {
        if (key[i] != '_') {
            break;
        }
        if(i > 2) {
            break;
        }
    }
    return (i == 2)?TRUE:FALSE;
}

/***************************************************************************
 *  Private key (variable) has a prefix of 1 underscore
 ***************************************************************************/
PUBLIC BOOL is_private_key(const char *key)
{
    if(!key) {
        return FALSE;
    }
    int i;
    for(i = 0; i < strlen(key); i++) {
        if (key[i] != '_') {
            break;
        }
        if(i > 2) {
            break;
        }
    }
    return (i == 1)?TRUE:FALSE;
}

/***************************************************************************
    Utility for databases of json records.
    Get a json list or dict, get the **first** record that match `id`
    WARNING `id` is the first key of json_desc
    Convention:
        - If it's a list of dict: the records have "id" field as primary key
        - If it's a dict, the key is the `id`
 ***************************************************************************/
PUBLIC json_t *kwjr_get( // Return is NOT yours, unless use of KW_EXTRACT
    json_t *kw,  // NOT owned
    const char *id,
    json_t *new_record,  // owned
    const json_desc_t *json_desc,
    size_t *idx_,      // If not null set the idx in case of array
    kw_flag_t flag
)
{
    json_t *v = NULL;
    BOOL backward = flag & KW_BACKWARD;

    if(idx_) { // error case
        *idx_ = 0;
    }

    if(!(json_is_array(kw) || json_is_object(kw))) {
        log_error(LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw must be dict or list",
            NULL
        );
        JSON_DECREF(new_record)
        return NULL;
    }
    if(empty_string(id)) {
        log_error(LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "id NULL",
            NULL
        );
        JSON_DECREF(new_record)
        return NULL;
    }

    switch(json_typeof(kw)) {
    case JSON_OBJECT:
        v = json_object_get(kw, id);
        if(!v) {
            if((flag & KW_CREATE)) {
                json_t *jn_record = create_json_record(json_desc);
                json_object_update_new(jn_record, json_deep_copy(new_record));
                json_object_set_new(kw, id, jn_record);
                JSON_DECREF(new_record)
                return jn_record;
            }
            if(flag & KW_REQUIRED) {
                log_error(LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "record NOT FOUND",
                    "id",           "%s", id,
                    NULL
                );
                log_debug_json(0, kw, "record NOT FOUND");
            }
            JSON_DECREF(new_record)
            return NULL;
        }
        if(flag & KW_EXTRACT) {
            json_incref(v);
            json_object_del(kw, id);
        }

        JSON_DECREF(new_record)
        return v;

    case JSON_ARRAY:
        {
            const char *pkey = json_desc->name;
            if(!backward) {
                size_t idx;
                json_array_foreach(kw, idx, v) {
                    const char *id_ = json_string_value(json_object_get(v, pkey));
                    if(id_ && strcmp(id_, id)==0) {
                        if(idx_) {
                            *idx_ = idx;
                        }
                        JSON_DECREF(new_record)
                        if(flag & KW_EXTRACT) {
                            json_incref(v);
                            json_array_remove(kw, idx);
                        }
                        return v;
                    }
                }
            } else {
                int idx;
                json_array_backward(kw, idx, v) {
                    const char *id_ = json_string_value(json_object_get(v, pkey));
                    if(id_ && strcmp(id_, id)==0) {
                        if(idx_) {
                            *idx_ = idx;
                        }
                        JSON_DECREF(new_record)
                        if(flag & KW_EXTRACT) {
                            json_incref(v);
                            json_array_remove(kw, idx);
                        }
                        return v;
                    }
                }
            }

            if((flag & KW_CREATE)) {
                json_t *jn_record = create_json_record(json_desc);
                json_object_update_new(jn_record, json_deep_copy(new_record));
                json_array_append_new(kw, jn_record);
                JSON_DECREF(new_record)
                return jn_record;
            }

            if(flag & KW_REQUIRED) {
                log_error(LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "record NOT FOUND",
                    "id",           "%s", id,
                    NULL
                );
                log_debug_json(0, kw, "record NOT FOUND");
            }
            JSON_DECREF(new_record)
            return NULL;
        }
        break;
    default:
        log_error(LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw must be dict or list",
            "id",           "%s", id,
            NULL
        );
        break;
    }

    JSON_DECREF(new_record)
    return NULL;
}
