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
PRIVATE json_t *_kw_search_path(json_t *kw, const char *path);
PRIVATE json_t *_kw_search_dict(json_t *kw, const char *path, kw_flag_t flag);


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
    if(!kw || kw->refcount <= 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD kw_incref()",
            NULL
        );
        return kw;
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
    if(!kw || kw->refcount <= 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD kw_decref()",
            NULL
        );
        return kw;
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
 *  Return the dict's value searched by path
 *  Last item in path is the key (of previous dict)!
 ***************************************************************************/
PRIVATE json_t *_kw_search_path(json_t *kw, const char *path)
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
        return json_object_get(kw, path);
    }

    char segment[256];
    snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path);
    json_t *deep_dict = json_object_get(kw, segment);
    return _kw_search_path(deep_dict, p+1);
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
    snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path);
    json_t *node_dict = json_object_get(kw, segment);
    if(!node_dict && (flag & KW_CREATE) && kw) {
        node_dict = json_object();
        json_object_set_new(kw, segment, node_dict);
    }
    return _kw_search_dict(node_dict, p+1, flag);
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
    if(_kw_search_path(kw, path)) {
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
    snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path);
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
    json_t *jn_dict = _kw_search_path(kw, path);
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
            log_debug_json(0, kw, "path '%s' NOT FOUND, default value returned", path);
        }
        return default_value;
    }
    if(!json_is_object(jn_dict)) {
        if(!(flag & KW_DONT_LOG)) {
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
    json_t *jn_list = _kw_search_path(kw, path);
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
            log_debug_json(0, kw, "path '%s' NOT FOUND, default value returned", path);
        }
        return default_value;
    }
    if(!json_is_array(jn_list)) {
        if(!(flag & KW_DONT_LOG)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json list",
                "path",         "%s", path,
                NULL
            );
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
    json_t *jn_int = _kw_search_path(kw, path);
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
            log_debug_json(0, kw, "path '%s' NOT FOUND, default value returned", path);
        }
        return default_value;
    }
    if(!json_is_integer(jn_int)) {
        if(!(flag & KW_WILD_NUMBER)) {
            if(!(flag & KW_DONT_LOG)) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "path MUST BE a json integer",
                    "path",         "%s", path,
                    NULL
                );
            }
            if(!json_is_number(jn_int) && !json_is_boolean(jn_int)) {
                return default_value;
            }
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
        if(*v == '0') {
            value = strtoll(v, 0, 8);
        } else if(*v == 'x' || *v == 'X') {
            value = strtoll(v, 0, 16);
        } else {
            value = strtoll(v, 0, 10);
        }
    } else if(json_is_null(jn_int)) {
        value = 0;
    } else {
        if(!(flag & KW_DONT_LOG)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a simple json element",
                "path",         "%s", path,
                NULL
            );
        }
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
    json_t *jn_real = _kw_search_path(kw, path);
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
            log_debug_json(0, kw, "path '%s' NOT FOUND, default value returned", path);
        }
        return default_value;
    }
    if(!json_is_real(jn_real)) {
        if(!(flag & KW_WILD_NUMBER)) {
            if(!(flag & KW_DONT_LOG)) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "path MUST BE a json real",
                    "path",         "%s", path,
                    NULL
                );
            }
            if(!json_is_number(jn_real) && !json_is_boolean(jn_real)) {
                return default_value;
            }
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
        if(!(flag & KW_DONT_LOG)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a simple json element",
                "path",         "%s", path,
                NULL
            );
        }
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
    json_t *jn_bool = _kw_search_path(kw, path);
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
            log_debug_json(0, kw, "path '%s' NOT FOUND, default value returned", path);
        }
        return default_value;
    }
    if(!json_is_boolean(jn_bool)) {
        if(!(flag & KW_WILD_NUMBER)) {
            if(!(flag & KW_DONT_LOG)) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "path MUST BE a json bool",
                    "path",         "%s", path,
                    NULL
                );
            }
            if(!json_is_number(jn_bool) && !json_is_boolean(jn_bool)) {
                return default_value;
            }
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
        if(!(flag & KW_DONT_LOG)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a simple json element",
                "path",         "%s", path,
                NULL
            );
        }
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
    json_t *jn_str = _kw_search_path(kw, path);
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
            log_debug_json(0, kw, "path '%s' NOT FOUND, default value returned", path);
        }
        return default_value;
    }
    if(!json_is_string(jn_str)) {
        if(!json_is_null(jn_str)) {
            if(!(flag & KW_DONT_LOG)) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "path MUST BE a json str",
                    "path",         "%s", path,
                    NULL
                );
            }
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
    json_t *jn_value = _kw_search_path(kw, path);
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
            log_debug_json(0, kw, "path '%s' NOT FOUND, default value returned", path);
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
        return -1;
    }

    char *p = search_delimiter(path, delimiter[0]);
    if(p) {
        char segment[1024];
        snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path);
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
    return json_object_set_new(kw, path, value);
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
                "key",          "%s", key,
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
        if(!(flag & KW_DONT_LOG)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "kw MUST BE a json array",
                NULL
            );
        }
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
PUBLIC const char *kw_get_list_str(json_t *list, size_t idx)
{
    json_t *jn_str;
    const char *str;

    if(!json_is_array(list)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL);
        return 0;
    }

    jn_str = json_array_get(list, idx);
    if(jn_str && !json_is_string(jn_str)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "value MUST BE a json string",
            NULL);
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
        snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path);
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
PUBLIC int64_t jn2integer(json_t *jn_var)
{
    int64_t val = 0;
    if(json_is_real(jn_var)) {
        val = (int64_t)json_real_value(jn_var);
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
    HACK Convention: all no-persistent metadata begins with "__".
    This function return a duplicate of kw removing all no-persistent metadata
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
    HACK Convention: no-persistent metadata begins with "__".
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
    It's not a deep copy, new keys are the paths.
    Not valid with lists.
    If paths are empty return kw
 ***************************************************************************/
PUBLIC json_t *kw_clone_by_keys(
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
)
{
    json_t *kw_clone = json_object();
    if(json_is_string(keys)) {
        const char *key = json_string_value(keys);
        json_t *jn_value = kw_get_dict(kw, key, 0, 0);
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
    } else if(json_is_object(keys)) {
        const char *key;
        json_t *jn_v;
        json_object_foreach(keys, key, jn_v) {
            json_t *jn_value = kw_get_dict(kw, key, 0, 0);
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
    } else if(json_is_array(keys)) {
        int idx;
        json_t *jn_v;
        json_array_foreach(keys, idx, jn_v) {
            const char *key = json_string_value(jn_v);
            json_t *jn_value = kw_get_dict(kw, key, 0, 0);
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

    KW_DECREF(kw);
    KW_DECREF(keys);
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
        int64_t val1 = jn2integer(jn_var1);
        int64_t val2 = jn2integer(jn_var2);
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
        int64_t val1 = jn2integer(jn_var1);
        int64_t val2 = jn2integer(jn_var2);
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
        return -1;
    }

    char *p = search_delimiter(path, delimiter[0]);
    if(p) {
        char segment[1024];
        snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path);
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
                return -1;
            }
        } else {
            json_t *node_dict = json_object_get(kw, segment);
            if(node_dict) {
                return kw_set_key_value(node_dict, pkey, p+1, value);
            } else {
                node_dict = json_object();
                json_object_set_new(kw, segment, node_dict);
                return kw_set_key_value(node_dict, pkey, p+1, value);
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
    const char *options, // "verbose", "lower"
    json_t *kw,  // NOT owned
    char *path
)
{
    BOOL verbose = FALSE;
    if(options && strstr(options, "verbose")) {
        verbose = TRUE;
    }

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
PUBLIC json_t *kwid_get(
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

    return jn;
}

/***************************************************************************
    Utility for databases.
    Return a new list from a "dict of records" or "list of records"
    WARNING the "id" of a dict's record is hardcorded to his key.
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'
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
        return FALSE;
    }
    switch(json_typeof(ids)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(ids, idx, jn_value) {
                const char *value = json_string_value(jn_value);
                if(value && strcmp(value, id)==0)  {
                    return TRUE;
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *key; json_t *jn_value;
            json_object_foreach(ids, key, jn_value) {
                if(strcmp(key, id)==0)  {
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
    Being `kw` a list of dicts [{},...] or a dict of dicts {id:{},...}
    return a new list of incref (clone) kw filtering the rows by `jn_filter` (where),
    If match_fn is 0 then kw_match_simple is used.
    NOTE Using JSON_INCREF/JSON_DECREF
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
            if(!kwid_match_id(ids, kw_get_str(jn_value, "id", 0, 0))) {
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
            if(match_fn(kw, jn_filter)) {
                json_array_append(kw_new, kw);
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
    Being field `name` of `kw` a list of ids [id,...] or a dict of ids {id:true,...} or a string id
    return a new list of all ids
 ***************************************************************************/
PUBLIC json_t *kwid_get_new_ids(
    json_t *kw, // not owned
    const char *name
)
{
    json_t *ids = kw_get_dict_value(kw, name, 0, KW_REQUIRED);
    if(!ids) {
        return 0;
    }
    json_t *new_ids = json_array();

    switch(json_typeof(ids)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(ids, idx, jn_value) {
                const char *value = json_string_value(jn_value);
                if(value)  {
                    json_array_append_new(new_ids, json_string(value));
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *key; json_t *jn_value;
            json_object_foreach(ids, key, jn_value) {
                json_array_append_new(new_ids, json_string(key));
            }
        }
        break;

    case JSON_STRING:
        json_array_append_new(new_ids, json_string(json_string_value(ids)));
        break;

    default:
        break;

    }

    return new_ids;
}

/***************************************************************************
 *  Check deeply the refcount of kw
 ***************************************************************************/
PUBLIC BOOL kw_check_refcounts(json_t *kw, int max_refcount) // not owned
{
    if(!kw) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw NULL",
            NULL
        );
        return FALSE;
    }

    switch(json_typeof(kw)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(kw, idx, jn_value) {
                if(!kw_check_refcounts(jn_value, max_refcount)) {
                    return FALSE;
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *key; json_t *jn_value;
            json_object_foreach(kw, key, jn_value) {
                if(!kw_check_refcounts(jn_value, max_refcount)) {
                    return FALSE;
                }
            }
        }
        break;

    case JSON_INTEGER:
    case JSON_REAL:
    case JSON_STRING:
        if(kw->refcount <= 0) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "refcount <= 0",
                "refcount",     "%d", (int)kw->refcount,
                NULL
            );
            return FALSE;
        }
        if(max_refcount > 0 && kw->refcount > max_refcount) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "refcount > max_refcount",
                "refcount",     "%d", (int)kw->refcount,
                "max_refcount", "%d", (int)max_refcount,
                NULL
            );
            return FALSE;
        }
        break;
    case JSON_TRUE:
    case JSON_FALSE:
    case JSON_NULL:
        // These have -1 refcount
        break;
    default:
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json corrupted",
            "refcount",     "%d", (int)kw->refcount,
            "max_refcount", "%d", (int)max_refcount,
            NULL
        );
        return FALSE;
    }

    return TRUE;
}
