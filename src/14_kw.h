/****************************************************************************
 *          KW.H
 *          Kw functions
 *          Copyright (c) 2013-2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/

#ifndef _KW_H
#define _KW_H 1

#include <jansson.h>

/*
 *  Dependencies
 */
#include "13_json_helper.h"
#include "12_gstrings2.h"

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Constants
 *********************************************************************/
#define KW_GET(__name__, __default__, __func__) \
    __name__ = __func__(kw, #__name__, __default__, 0);

#define KW_GET_REQUIRED(__name__, __default__, __func__) \
__name__ = __func__(kw, #__name__, __default__, KW_REQUIRED);


/*********************************************************************
 *      Prototypes
 *********************************************************************/
typedef enum {
    KW_REQUIRED         = 0x0001,   // Log error message if not exist.
    KW_CREATE           = 0x0002,   // Create if not exist
    KW_WILD_NUMBER      = 0x0004,   // For numbers work indistinctly with real/int/bool/string without error logging
    KW_EXTRACT          = 0x0008,   // Extract (delete) the key on read from dictionary.
    KW_DONT_LOG         = 0x0010,   // Don't log errors when types are wrong and returning default
    KW_EMPTY_VALID      = 0x0020,   // Empty strings are valid.
} kw_flag_t;

typedef void (*incref_fn_t)(void *);
typedef void (*decref_fn_t)(void *);
typedef json_t * (*serialize_fn_t)(void *ptr);
typedef void * (*deserialize_fn_t)(json_t *jn);

/*********************************************************************
 *      Macros
 *********************************************************************/
#define KW_DECREF(ptr)      \
    if(ptr) {               \
        kw_decref(ptr);     \
        (ptr) = 0;          \
    }

#define KW_INCREF(ptr)      \
    if(ptr) {               \
        kw_incref(ptr);     \
    }


/*********************************************************************
 *      Prototypes
 *********************************************************************/
/**rst**
   Xxx
**rst**/
PUBLIC int kw_add_binary_type(
    const char *binary_field_name,
    const char *serialized_field_name,
    serialize_fn_t serialize_fn,
    deserialize_fn_t deserialize_fn,
    incref_fn_t incref_fn,
    decref_fn_t decref_fn
);

/**rst**
   kw_clean_binary_types
**rst**/
PUBLIC void kw_clean_binary_types(void);

/**rst**
   Return a new kw serialized of `kw`.
   (to send it outside of yuno)
**rst**/
PUBLIC json_t *kw_serialize(
    json_t *kw  // owned
);

/**rst**
    Return a new kw deserialized of`kw`.
    (to incorporate from outside of yuno)
**rst**/
PUBLIC json_t *kw_deserialize(
    json_t *kw // owned
);

/**rst**
   This functions does json_incref
   but also it does incref of binary fields like gbuffer.
   Return the input parameter.
**rst**/
PUBLIC json_t *kw_incref(json_t *kw);

/**rst**
   This functions does json_decref
   but also it does decref of binary fields like gbuffer.
   Return the input parameter.
**rst**/
PUBLIC json_t *kw_decref(json_t *kw);

/**rst**
    Return the json's value find by path, walking over LISTS and DICTS
**rst**/
PUBLIC json_t *kw_find_path(json_t *kw, const char *path, BOOL verbose);

/**rst**
   Return TRUE if the dictionary ``kw`` has the key ``key``.
**rst**/
PUBLIC BOOL kw_has_key(json_t *kw, const char *key);

/**rst**
   Return TRUE if the dictionary ``path`` of ``kw`` has the key ``key``.
**rst**/
PUBLIC BOOL kw_has_subkey(json_t *kw, const char *path, const char *key);

/**rst**
   Set delimiter. Default is '`'
**rst**/
PUBLIC char kw_set_path_delimiter(char delimiter);

/**rst**
   Return TRUE if the dictionary ``kw`` has the path ``path``.
**rst**/
PUBLIC BOOL kw_has_path(json_t *kw, const char *path);

/**rst**
   Delete key
**rst**/
PUBLIC int kw_delete_subkey(json_t *kw, const char *path, const char *key);


/**rst**
   Delete the dict's value searched by path
**rst**/
PUBLIC int kw_delete(json_t *kw, const char *path);

/**rst**
   Return the ``path` dict value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_t *kw_get_dict(
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` list value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_t *kw_get_list(
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` int value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_int_t kw_get_int(
    json_t *kw,
    const char *path,
    json_int_t default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` real value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC double kw_get_real(
    json_t *kw,
    const char *path,
    double default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` boolean value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC BOOL kw_get_bool(
    json_t *kw,
    const char *path,
    BOOL default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` string value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC const char *kw_get_str(
    json_t *kw,
    const char *path,
    const char *default_value,
    kw_flag_t flag
);

/**rst**
   Get any value from an json object searched by path.
   Return the ``path` json value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_t *kw_get_dict_value(
    json_t *kw,
    const char *path,
    json_t *default_value,  // owned
    kw_flag_t flag
);

/**rst**
   Like json_object_set but with a path.
**rst**/
PUBLIC int kw_set_dict_value(
    json_t *kw,
    const char *path,   // The last word after . is the key
    json_t *value // owned
);

/**rst**
**rst**/
PUBLIC json_t *kw_get_subdict_value(
    json_t *kw,
    const char *path,
    const char *key,
    json_t *jn_default_value,  // owned
    kw_flag_t flag
);

/**rst**
**rst**/
PUBLIC int kw_set_subdict_value(
    json_t* kw,
    const char *path,
    const char *key,
    json_t* value // owned
);

/**rst**
   Get the value in idx position from an json array .
   Return the ``idx` json value from the ``kw`` list.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
**rst**/
PUBLIC json_t *kw_get_list_value(
    json_t* kw,
    int idx,
    kw_flag_t flag
);

/**rst**
   Get list value that must be a string
**rst**/
PUBLIC const char *kw_get_list_str(json_t *list, size_t idx); // backward V1

/**rst**
   like json_array_append but with a path into a dict
**rst**/
PUBLIC int kw_list_append(
    json_t *kw,
    const char *path,
    json_t *value
);

/**rst**
    Return a C binary int array from json int list.
    ``int_array_len`` gets the size of int array.
    The returned value must be free with gbmem_free()
**rst**/
PUBLIC uint32_t * kw_get_bin_uint32_array(
    json_t *kw,
    const char *path,
    size_t *uint32_array_len
);

/**rst**
    Utility for databases.
    Extract (remove) "ids" and "id" keys from kw
    Return a new dict only with {"ids": [json_expand_integer_list(ids) | id]}
**rst**/
PUBLIC json_t *kwids_extract_and_expand_ids(
    json_t *kw  // NOT owned
);

/**rst**
    Utility for databases.
    Same as kw_extract_and_expand_ids()
    but it's required that must be only one id, otherwise return 0.
    Return a new dict only with {"ids": [ids[0] | id]}
**rst**/
PUBLIC json_t *kwids_extract_id(
    json_t *kw  // NOT owned
);

/**rst**
    Utility for databases.
    Return dict with {"ids": [json_expand_integer_list(jn_id)]}
**rst**/
PUBLIC json_t *kwids_json2kwids(
    json_t *jn_ids  // NOT owned
);

/**rst**
    Utility for databases.
    Return dict with {"ids": [id]}
    If id is 0 then return 0 (id cannot be 0)
**rst**/
PUBLIC json_t *kwids_id2kwids(
    json_int_t id
);

/**rst**
    Simple json to real
**rst**/
PUBLIC double jn2real(
    json_t *jn_var
);

/**rst**
    Simple json to int
**rst**/
PUBLIC int64_t jn2integer(
    json_t *jn_var
);

/**rst**
    Simple json to string, WARNING free return with gbmem_free
**rst**/
PUBLIC char *jn2string(
    json_t *jn_var
);

/**rst**
    Simple json to boolean
**rst**/
PUBLIC BOOL jn2bool(
    json_t *jn_var
);

PUBLIC int kw_walk(
    json_t *kw, // not owned
    int (*callback)(json_t *kw, const char *key, json_t *value)
);

/************************************************************************
    WARNING

    **duplicate** is a copy with new references

    **clone** is a copy with incref references

 ************************************************************************/

/**rst**
   Make a duplicate of kw
   WARNING near as json_deep_copy(), but processing serialized fields and with new references
**rst**/
PUBLIC json_t *kw_duplicate(
    json_t *kw  // NOT owned
);

/**rst**
    HACK Convention: private data begins with "_".
    This function return a duplicate of kw removing all private data
**rst**/
PUBLIC json_t *kw_filter_private(
    json_t *kw  // owned
);

/**rst**
    HACK Convention: metadata begins with "__".
    This function return a duplicate of kw removing all metadata
**rst**/
PUBLIC json_t *kw_filter_metadata(
    json_t *kw  // owned
);

/**rst**
    HACK Convention: private data begins with "_".
    Delete private keys (only first level)
**rst**/
PUBLIC int kw_delete_private_keys(
    json_t *kw  // NOT owned
);

/**rst**
    HACK Convention: metadata begins with "__".
    Delete metadata keys (only first level)
**rst**/
PUBLIC int kw_delete_metadata_keys(
    json_t *kw  // NOT owned
);

/**rst**
    Return a new kw only with the keys.
    If `keys` is null then return a duplicate of kw.
    A key can be repeated by the tree.
**rst**/
PUBLIC json_t *kw_duplicate_with_only_keys(
    json_t *kw,     // NOT owned
    const char **keys
);

/**rst**
    Return a new kw only with the keys got by path.
    It's not a deep copy, new keys are the paths.
    Not valid with lists.
    If paths are empty return kw
**rst**/
PUBLIC json_t *kw_clone_by_path(
    json_t *kw,     // owned
    const char **paths
);

/**rst**
    Return a new kw only with the keys got by dict's keys or list's keys (strings).
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

    It's not a deep copy, new keys are the paths.
    If paths are empty return kw
**rst**/
PUBLIC json_t *kw_clone_by_keys(
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
);

/**rst**
    Update kw with other, except keys in except_keys
    First level only
**rst**/
PUBLIC void kw_update_except(
    json_t *kw,  // NOT owned
    json_t *other,  // owned
    const char **except_keys
);

/**rst**
    Compare two json and return TRUE if they are identical.
**rst**/
PUBLIC BOOL kw_is_identical(
    json_t *kw1,    // NOT owned
    json_t *kw2     // NOT owned
);

/**rst**
    Only compare str/int/real/bool items
    Complex types are done as matched
    Return lower, iqual, higher (-1, 0, 1), like strcmp
**rst**/
PUBLIC int cmp_two_simple_json(
    json_t *jn_var1,    // NOT owned
    json_t *jn_var2     // NOT owned
);

/**rst**
    Match a json dict with a json filter
    Only compare str/int/real/bool items
**rst**/
PUBLIC BOOL kw_match_simple(
    json_t *kw,         // NOT owned
    json_t *jn_filter   // owned
);
typedef BOOL (*kw_match_fn)(
    json_t *kw,         // NOT owned
    json_t *jn_filter   // owned
);

/**rst**
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of **duplicated** kw filtering the rows by `jn_filter` (where),
    and returning the `keys` fields of row (select).
    If match_fn is 0 then kw_match_simple is used.
**rst**/
PUBLIC json_t *kw_select( // WARNING return **duplicated** objects
    json_t *kw,         // NOT owned
    const char **keys,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // NOT owned
        json_t *jn_filter   // owned
    )
);

/**rst**
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of **duplicated** kw filtering the rows by `jn_filter` (where),
    and returning the `keys` fields of row (select).
    If match_fn is 0 then kw_match_simple is used.
**rst**/
// TODO sin probar
PUBLIC json_t *kw_select2( // WARNING return **clone** objects
    json_t *kw,         // NOT owned
    json_t *jn_keys,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // NOT owned
        json_t *jn_filter   // owned
    )
);

/**rst**
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of incref (clone) kw filtering the rows by `jn_filter` (where),
    If match_fn is 0 then kw_match_simple is used.
    NOTE Using JSON_INCREF/JSON_DECREF
**rst**/
PUBLIC json_t *kw_collect( // WARNING be care, you can modify the original records
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);

/**rst**
    Navigate by deep dicts of kw.
    Arrays are navigated if contains `pkey` (id) field (string or integer ONLY).
**rst**/
PUBLIC int kw_navigate(
    json_t *kw,         // NOT owned
    const char *pkey,
    int (*navigate_callback)(
        const char *path, json_t *kw, void *param1, void *param2, void *param3
    ),
    void *param1,
    void *param2,
    void *param3
);

/**rst**
    Return a new kw with values of propagated `keys`
    being the returned keys the full paths of `keys` instances.
    Arrays of dicts are identify by the `pkey` (id) field.
**rst**/
PUBLIC json_t *kw_get_propagated_key_values(
    json_t *kw,         // NOT owned
    const char *pkey,
    const char **keys
);

/**rst**
    Restore values of propagated `keys`
**rst**/
PUBLIC int kw_put_propagated_key_values(
    json_t *kw,         // NOT owned
    const char *pkey,
    json_t *propagated_keys
);

/**rst**
    Like kw_set_dict_value but managing arrays of dicts with `pkey` (id) field
**rst**/
PUBLIC int kw_set_key_value(
    json_t *kw,
    const char *pkey,
    const char *path,   // The last word after . is the key
    json_t *value // owned
);

/**rst**
    Get the dict of dict's array with `pkey` field (id)
**rst**/
json_t *kw_get_record(
    json_t *dict_list, // NOT owned
    const char *pkey,
    json_t *pkey_value  // owned
);

/**rst**
    Insert dict in dict's array ordered by field pkey
**rst**/
int kw_insert_ordered_record(
    json_t *dict_list, // NOT owned
    const char *pkey,
    json_t *record  // owned
);

/**rst**
    HACK el eslabón perdido.
    Return a new kw applying __json_config_variables__
**rst**/
PUBLIC json_t *kw_apply_json_config_variables(
    json_t *kw,          // NOT owned
    json_t *jn_global    // NOT owned
);

/**rst**
    Remove from kw1 all keys in kw2
    kw2 can be a string, dict, or list.
**rst**/
PUBLIC int kw_pop(
    json_t *kw1, // NOT owned
    json_t *kw2  // NOT owned
);

/**rst**
    Has word? Search in string, list or dict.
    options: "recursive", "verbose"

    Use to configurate:

        "opt2"              No has word "opt1"
        "opt1|opt2"         Yes, has word "opt1"
        ["opt1", "opt2"]    Yes, has word "opt1"
        {
            "opt1": true,   Yes, has word "opt1"
            "opt2": false   No has word "opt1"
        }
**rst**/
PUBLIC BOOL kw_has_word(
    json_t *kw,  // NOT owned
    const char *word,
    const char *options // "recursive", "verbose"
);

/**rst**
    Utility for databases.
    Get a json item walking by the tree (routed by path)
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'
**rst**/
PUBLIC json_t *kwid_get( // Return is NOT YOURS
    const char *options, // "verbose", "lower", "backward"
    json_t *kw,  // NOT owned
    const char *path,
    ...
);

/**rst**
    Utility for databases.
    Return a new list from a "dict of records" or "list of records"
    WARNING the "id" of a dict's record is hardcorded to their key.
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'
    If path is empty then use kw
**rst**/
PUBLIC json_t *kwid_new_list(
    const char *options, // "verbose", "lower"
    json_t *kw,  // NOT owned
    const char *path,
    ...
);

/**rst**
    Utility for databases.
    Return a new dict from a "dict of records" or "list of records"
    WARNING the "id" of a dict's record is hardcorded to their key.
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'
    If path is empty then use kw
**rst**/
PUBLIC json_t *kwid_new_dict(
    const char *options, // "verbose", "lower"
    json_t *kw,  // NOT owned
    const char *path,
    ...
);

/**rst**
    Utility for databases.
    Return TRUE if `id` is in the list/dict/str `ids`
**rst**/
PUBLIC BOOL kwid_match_id(
    json_t *ids,
    const char *id
);

/**rst**
    Utility for databases.
    Return TRUE if `id` WITH LIMITED SIZE is in the list/dict/str `ids`
**rst**/
PUBLIC BOOL kwid_match_nid(
    json_t *ids,
    const char *id,
    int max_id_size
);

/**rst**
    Utility for databases.
    Being `kw` a:
        - list of strings [s,...]
        - list of dicts [{},...]
        - dict of dicts {id:{},...}
    return a **NEW** list of incref (clone) kw filtering the rows by `jn_filter` (where),
    and matching the ids.
    If match_fn is 0 then kw_match_simple is used.
    NOTE Using JSON_INCREF/JSON_DECREF HACK
**rst**/
PUBLIC json_t *kwid_collect( // WARNING be care, you can modify the original records
    json_t *kw,         // not owned
    json_t *ids,        // owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);

/**rst**
    Utility for databases.
    Being `ids` a:
        "$id"
        ["$id", ...]
        [{"id":$id, ...}, ...]
        {
            "$id": {}.
            ...
        }
    return a new list of all ids (all duplicated items)
**rst**/
PUBLIC json_t *kwid_get_ids(
    json_t *ids // not owned
);

/**rst**
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
**rst**/
PUBLIC json_t *kwid_get_id_records(
    json_t *records // not owned
);

/**rst**
    Check all refcounts
**rst**/
PUBLIC int kw_check_refcounts(
    json_t *kw, // not owned
    int max_refcount,
    int *result // firstly initalize to 0
);

/**rst**
    Utility for databases.
    Being field `kw` a list of id record [{id...},...] return the record idx with `id`
    Return -1 if not found
**rst**/
size_t kwid_find_record_in_list(
    const char *options, // "verbose"
    json_t *kw_list,
    const char *id
);

/**rst**
    Get a the idx of simple json item in a json list.
    Return -1 if not found
**rst**/
PUBLIC size_t kw_find_json_in_list(
    const char *options, // "verbose"
    json_t *kw_list,
    json_t *item
);

/**rst**
    Compare deeply two json **records**. Can be disordered.
**rst**/
PUBLIC BOOL kwid_compare_records(
    json_t *record, // NOT owned
    json_t *expected, // NOT owned
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
);

/**rst**
    Compare deeply two json lists of **records**. Can be disordered.
**rst**/
PUBLIC BOOL kwid_compare_lists(
    json_t *list, // NOT owned
    json_t *expected, // NOT owned
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
);

/**rst**
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
**rst**/
PUBLIC json_t *kwid_tree_select( // WARNING be care, you can modify the original kw
    json_t *kw,             // not owned
    json_t *jn_fields,      // owned
    const char *tree_hook,
    json_t *jn_filter,      // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    ),
    BOOL verbose
);

/**rst**
    Utility for databases.

    Return a new dict with all id-records found in `kw`
**rst**/
PUBLIC json_t *kwid_new_dict_tree_collect( // WARNING be care, you can modify the original kw
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);

/**rst**
    Utility for databases.

    Return a new list with all id-records found in `kw`
**rst**/
PUBLIC json_t *kwid_new_list_tree_collect( // WARNING be care, you can modify the original kw
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);

/**rst**
    Return a new json with all arrays or dicts greater than `limit` collapsed with
        __collapsed__: {
            path:
            size:
        }
    WARNING _limit must be > 0 to collapse
**rst**/
PUBLIC json_t *kw_collapse(
    json_t *kw,         // not owned
    int collapse_lists_limit,
    int collapse_dicts_limit
);


#ifdef __cplusplus
}
#endif


#endif

