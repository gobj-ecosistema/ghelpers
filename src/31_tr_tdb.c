/***********************************************************************
 *          TR_TDB.C
 *
 *          Tree Database with TimeRanger
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "31_tr_tdb.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
);
PRIVATE json_t *_trtdb_select(
    json_t *kw,         // not owned
    json_t *jn_fields,  // owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE json_t *topic_cols_desc = 0;

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trtdb_open_db( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_schema       // owned
)
{
    if(!jn_schema) {
        return 0;
    }

    /*--------------------------------*
     *      Create desc of cols
     *--------------------------------*/
    if(!topic_cols_desc) {
        topic_cols_desc = _trtdb_create_topic_cols_desc();
    } else {
        JSON_INCREF(topic_cols_desc);
    }

    /*
     *  At least 'topics' must be.
     */
    json_t *jn_topics = kw_get_list(jn_schema, "topics", 0, KW_REQUIRED);
    if(!jn_topics) {
        JSON_DECREF(jn_schema);
        return 0;
    }

    /*
     *  The tree is built in tranger, check if already exits
     */
    json_t *trtdb = kw_get_subdict_value(tranger, "treedbs", treedb_name, 0, 0);
    if(trtdb) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "TreeDB ALREADY opened.",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        JSON_DECREF(jn_schema);
        return 0;
    }

    /*
     *  Open/Create "system" topics
     */
    char tags_name[NAME_MAX];
    snprintf(tags_name, sizeof(tags_name), "__tags_%s__", treedb_name);

    json_t *tags_topic = tranger_create_topic(
        tranger,    // If topic exists then only needs (tranger,name) parameters
        tags_name,
        "id",
        "",
        sf_rowid_key,
        json_pack("[{s:s, s:s, s:s, s:[s,s]}, {s:s, s:s, s:s, s:[s,s]}]",
            "id", "id",
            "header", "Id",
            "type", "integer",
            "flag",
                "persistent","required",

            "id", "name",
            "header", "Name",
            "type", "string",
            "flag",
                "persistent", "required"
        )
    );

// print_json(topic_cols_desc);
// print_json(kw_get_list(tags_topic, "cols", 0, KW_REQUIRED));

    parse_schema_cols(tags_name, topic_cols_desc, kw_get_list(tags_topic, "cols", 0, KW_REQUIRED));

    /*
     *  Open/Create "user" topics
     */
    int idx;
    json_t *jn_desc;
    json_array_foreach(jn_topics, idx, jn_desc) {
        const char *topic_name = kw_get_str(jn_desc, "topic_name", "", 0);
        if(empty_string(topic_name)) {
            continue;
        }
        tranger_create_topic(
            tranger,    // If topic exists then only needs (tranger,name) parameters
            topic_name,
            kw_get_str(jn_desc, "pkey", "", 0),
            kw_get_str(jn_desc, "tkey", "", 0),
            tranger_str2system_flag(kw_get_str(jn_desc, "system_flag", "", 0)),
            json_incref(kw_get_list(jn_desc, "cols", 0, 0))
        );
    }

    /*
     *  Create the root of treedb
     */
    json_t *treedbs = kw_get_dict(tranger, "treedbs", json_object(), KW_CREATE);
    trtdb = kw_get_dict(treedbs, treedb_name, json_object(), KW_CREATE);

    /*
     *  Open "system" lists
     */
    json_t *jn_filter = json_pack("{}");
    json_t *jn_list = json_pack("{s:s, s:o, s:I}",
        "topic_name", tags_name,
        "match_cond", jn_filter,
        "load_record_callback", (json_int_t)(size_t)load_record_callback
    );
    tranger_open_list(
        tranger,
        jn_list // owned
    );

    kw_get_subdict_value(trtdb, "__tags__", "data", json_array(), KW_CREATE);
    kw_get_subdict_value(trtdb, "__tags__", "indexes", json_object(), KW_CREATE);

    /*
     *  Open "user" lists
     */
    json_array_foreach(jn_topics, idx, jn_desc) {
        const char *topic_name = kw_get_str(jn_desc, "topic_name", "", 0);
        if(empty_string(topic_name)) {
            continue;
        }
        json_t *jn_filter = json_pack("{}"); // TODO pon el current tag
        json_t *jn_list = json_pack("{s:s, s:o, s:I}",
            "topic_name", topic_name,
            "match_cond", jn_filter,
            "load_record_callback", (json_int_t)(size_t)load_record_callback
        );
        tranger_open_list(
            tranger,
            jn_list // owned
        );

        kw_get_subdict_value(trtdb, topic_name, "data", json_array(), KW_CREATE);
        kw_get_subdict_value(trtdb, topic_name, "indexes", json_object(), KW_CREATE);
    }

    JSON_DECREF(jn_schema);
    return trtdb;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int trtdb_close_db(
    json_t *tranger,
    const char *treedb_name
)
{
    json_t *trtdb = kw_get_subdict_value(tranger, "treedbs", treedb_name, 0, KW_EXTRACT);
    JSON_DECREF(trtdb);
    JSON_DECREF(topic_cols_desc);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int trtdb_tag( // tag the current tree db
    json_t *tranger,
    const char *treedb_name,
    const char *tag
)
{
    // TODO
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int trtdb_reopen_db(
    json_t *tranger,
    const char *treedb_name,
    const char *tag  // If empty tag then free the tree, active record will be the last record.
)
{
    // TODO
    return 0;
}




                    /*------------------------------------*
                     *      Testing
                     *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL validate_topic(
    json_t *topic,
    json_t *record,
    const char *options
)
{
    return TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *_trtdb_create_topic_cols_desc(void)
{
    json_t *topic_cols_desc = json_array();
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:[s,s], s:[s,s]}",
            "id", "id",
            "header", "Id",
            "type",
                "string", "integer",
            "flag",
                "required", "pkey"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:s, s:s}",
            "id", "header",
            "header", "Header",
            "type",
                "string",
            "flag",
                ""
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:s, s:[s,s,s,s,s,s,s,s], s:s}",
            "id", "type",
            "header", "Type",
            "type", "enum",
            "enum",
                "string","integer","object","array","real","boolean","null",  "enum",
            "flag",
                "required"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:s, s:[s,s,s,s,s,s,s,s,s], s:s}",
            "id", "flag",
            "header", "Flag",
            "type", "enum",
            "enum",
                "","persistent","required","volatil","pkey","uuid","link","reverse","include",
            "flag",
                ""
        )
    );
    return topic_cols_desc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *my_json_type(json_t *field)
{
    if(json_is_string(field)) {
        if(strcasecmp(json_string_value(field), "enum")==0) {
            return "enum";
        } else {
            return "string";
        }
    } else if(json_is_integer(field)) {
        return "integer";
    } else if(json_is_object(field)) {
        return "object";
    } else if(json_is_array(field)) {
        return "array";
    } else if(json_is_boolean(field)) {
        return "boolean";
    } else if(json_is_real(field)) {
        return "real";
    } else if(json_is_null(field)) {
        return "null";
    } else {
        return "merde";
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_field(const char *topic_name, json_t *desc, json_t *data)
{
    int ret = 0;

// printf("======>\n"); // TODO TEST
// print_json(desc);
// print_json(data);

    /*
     *  Get description of field
     */
    const char *desc_id = kw_get_str(desc, "id", 0, 0);
    json_t *desc_type = kw_get_dict_value(desc, "type", 0, 0);
    json_t *desc_flag = kw_get_dict_value(desc, "flag", 0, 0);

    if(!desc_id) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Field 'id' required",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }
    if(!desc_type) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Field 'type' required",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    /*
     *  Get value and type
     */
    json_t *value = kw_get_dict_value(data, desc_id, 0, 0);
    const char *value_type = my_json_type(value);

    /*
     *  Check field required
     */
    if(kw_has_word(desc_flag, "required", 0)) {
        if(!value) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Field required",
                "topic",        "%s", topic_name,
                "field",        "%s", desc_id,
                NULL
            );
            return -1;
        }
    }

    /*
     *  Check value type
     */
    const char *my_desc_type = my_json_type(desc_type);
    SWITCHS(my_desc_type) {
        CASES("enum")
            json_t *desc_enum = kw_get_list(desc, "enum", 0, 0);
            switch(json_typeof(value)) {
            case JSON_ARRAY:
                {
                    int idx; json_t *v;
                    json_array_foreach(value, idx, v) {
                        switch(json_typeof(v)) {
                        case JSON_STRING:
                            if(!json_str_in_list(desc_enum, json_string_value(v), TRUE)) {
                                log_error(0,
                                    "gobj",         "%s", __FILE__,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "Wrong enum type",
                                    "topic",        "%s", topic_name,
                                    "field",        "%s", desc_id,
                                    "value",        "%s", json_string_value(v),
                                    "value_to_be",  "%j", desc_enum,
                                    NULL
                                );
                                ret += -1;
                            }
                            break;
                        default:
                            log_error(0,
                                "gobj",         "%s", __FILE__,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "Case not implemented",
                                "topic",        "%s", topic_name,
                                "field",        "%s", desc_id,
                                "value",        "%s", json_string_value(v),
                                "value_to_be",  "%j", desc_enum,
                                NULL
                            );
                            ret += -1;
                            break;
                        }

                    }
                }
                break;
            case JSON_STRING:
                if(!json_str_in_list(desc_enum, json_string_value(value), TRUE)) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "Wrong enum type",
                        "topic",        "%s", topic_name,
                        "field",        "%s", desc_id,
                        "value",        "%s", json_string_value(value),
                        "value_to_be",  "%j", desc_enum,
                        NULL
                    );
                    ret += -1;
                }
                break;
            default:
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Enum value must be string or string's array",
                    "topic",        "%s", topic_name,
                    "field",        "%s", desc_id,
                    "value",        "%s", json_string_value(value),
                    "value_to_be",  "%j", desc_enum,
                    NULL
                );
                ret += -1;
                break;
            }
            break;

        // Json basic types
        DEFAULTS
            if(!kw_has_word(desc_type, value_type, 0)) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Wrong basic type",
                    "topic",        "%s", topic_name,
                    "field",        "%s", desc_id,
                    "value",        "%s", value_type,
                    "value_to_be",  "%j", desc_type,
                    NULL
                );
                ret += -1;
            }
            break;
    } SWITCHS_END;

    return ret;
}

/***************************************************************************
 *  Return 0 if ok or # of errors in negative
 ***************************************************************************/
PUBLIC int parse_schema_cols(
    const char *topic_name,
    json_t *cols_desc,  // not owned
    json_t *data  // not owned
)
{
    int ret = 0;

    if(!(json_is_object(data) || json_is_array(data))) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Data must be an {} or [{}]",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    int idx; json_t *desc;

    if(json_is_object(data)) {
        json_array_foreach(cols_desc, idx, desc) {
            ret += check_field(topic_name, desc, data);
        }
    } else if(json_is_array(data)) {
        int idx1; json_t *d;
        json_array_foreach(data, idx1, d) {
            int idx2;
            json_array_foreach(cols_desc, idx2, desc) {
                ret += check_field(topic_name, desc, d);
            }
        }
    }

    return ret;
}




                    /*------------------------------------*
                     *      Manage the tree's nodes
                     *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_id(
    json_t *topic,
    json_t *id,     // not owned
    json_t *record  // not owned
)
{
    const char *pkey = kw_get_str(topic, "pkey", "", KW_REQUIRED);

    json_t *cols = kw_select(
        kw_get_list(topic, "cols", 0, KW_REQUIRED),
        0,
        json_pack("{s:s}",
            "id", pkey
        ),
        0
    );
    json_t *col = kw_get_list_value(cols, 0, KW_REQUIRED);
    if(!col) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot get pkey col desc",
            "pkey",         "%s", pkey,
            NULL
        );
        JSON_DECREF(col);
        return -1;
    }

    const char *type = kw_get_str(col, "type", 0, KW_REQUIRED);
    if(!type) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Col desc without type",
            "pkey",         "%s", pkey,
            NULL
        );
        JSON_DECREF(col);
        return -1;
    }

    SWITCHS(type) {
        CASES("string")
            char *v = jn2string(id);
            json_object_set_new(record, pkey, json_string(v));
            gbmem_free(v);
            break;
        CASES("integer")
            json_int_t v = jn2integer(id);
            json_object_set_new(record, pkey, json_integer(v));
            break;
        DEFAULTS
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Col type unknown",
                "pkey",         "%s", pkey,
                "type",         "%s", type,
                NULL
            );
            JSON_DECREF(col);
            return -1;
    } SWITCHS_END;

    JSON_DECREF(col);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trtdb_read_node( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id,     // owned, Can be: integer,string, [integer], [string], [keys]
    json_t *fields, // owned, Return only this fields. Can be: string, [string], [keys]
    json_t *kw,     // owned, Being filter on reading or record on writting
    const char *options // "create", "delete", "critical", "verbose", "metadata"
)
{
    json_t *record = 0;

    /*-------------------------------*
     *      Get topic data
     *-------------------------------*/
    char path[NAME_MAX];
    snprintf(path, sizeof(path), "treedbs`%s`%s`data", treedb_name, topic_name);
    strtolower(path);
    json_t *data = kw_get_list(tranger, path, 0, KW_REQUIRED);
    if(!data) {
        if(options && strstr(options, "verbose")) {
            log_error(
                LOG_OPT_TRACE_STACK|
                    (options && strstr(options, "critical"))?LOG_OPT_EXIT_ZERO:0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "TreeDb Topic data NOT FOUND",
                "path",         "%s", path,
                NULL
            );
        }
        JSON_DECREF(id);
        JSON_DECREF(fields);
        JSON_DECREF(kw);
        return 0;
    }

    if(id) {
        /*-------------------------------*
         *      Working with id
         *-------------------------------*/
        JSON_INCREF(fields);
        JSON_INCREF(id);
        record = _trtdb_select(
            data,   // not owned
            fields, // owned, fields
            id,     // owned, filter
            0       // match fn
        );
        if(json_array_size(record)==0) {
            JSON_DECREF(record);
            if(options && strstr(options, "create")) {
                JSON_INCREF(id);
                JSON_INCREF(kw);
                record = trtdb_write_node(
                    tranger,
                    treedb_name,
                    topic_name,
                    id, // owned
                    kw, // owned
                    options // "inmediate"
                );
            }
        }
    } else {
        /*-------------------------------*
         *      Working without id
         *-------------------------------*/
        // TODO
    }

    // TODO if fields clone by fields else return original?


    JSON_DECREF(id);
    JSON_DECREF(fields);
    JSON_DECREF(kw);
    return record;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trtdb_write_node( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id, // owned
    json_t *kw, // owned
    const char *options // "inmediate"
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    json_t *record = 0;

    /*-------------------------------*
     *      Get topic data
     *-------------------------------*/
    char path[NAME_MAX];
    snprintf(path, sizeof(path), "treedbs`%s`%s`data", treedb_name, topic_name);
    strtolower(path);
    json_t *data = kw_get_list(tranger, path, 0, KW_REQUIRED);
    if(!data) {
        if(options && strstr(options, "verbose")) {
            log_error(
                LOG_OPT_TRACE_STACK|
                    (options && strstr(options, "critical"))?LOG_OPT_EXIT_ZERO:0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "TreeDb Topic data NOT FOUND",
                "path",         "%s", path,
                NULL
            );
        }
        JSON_DECREF(id);
        JSON_DECREF(kw);
        return 0;
    }

    // TODO check cols, id with uuid
    // parse_schema_cols(tranger_topic_name(topic), topic_cols_desc, json_array_get(col, 0));


    if(id) {
        /*-------------------------------*
         *      Working with id
         *-------------------------------*/

        /*
         *  Duplicate (new references) the kw to build the new recod
         */
        record = json_deep_copy(kw);
        set_id(topic, id, record);

        /*
         *  Validate record
         */
        if(!validate_topic(topic, record, options)) {
            // TODO log_error if verbose
            JSON_DECREF(record);
            JSON_DECREF(id);
            JSON_DECREF(kw);
            return 0;
        }

        /*
         *  Write record
         */
        JSON_INCREF(record);
        md_record_t md_record;
        if(tranger_append_record(
            tranger,
            topic_name,
            0, // __t__,         // if 0 then the time will be set by TimeRanger with now time
            0, // user_flag,
            &md_record, // md_record,
            record // owned
        )<0) {
            // Error already logged
            JSON_DECREF(record);
        }
    } else {
        /*-------------------------------*
         *      Working without id
         *-------------------------------*/
        // TODO
    }


    JSON_DECREF(id);
    JSON_DECREF(kw);
    return record;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
)
{
    json_t *jn_messages = kw_get_dict(list, "messages", 0, KW_REQUIRED);
    json_t *jn_filter2 = kw_get_dict(list, "match_cond", 0, KW_REQUIRED);

    char *key = md_record->key.s; // convierte las claves int a string
    char key_[64];
    if(md_record->__system_flag__ & (sf_int_key|sf_rowid_key)) {
        snprintf(key_, sizeof(key_), "%"PRIu64, md_record->key.i);
        key = key_;
    }

    /*
     *  Search the message for this key
     */
    json_t *message = kw_get_dict_value(jn_messages, key, json_object(), KW_CREATE);
    json_t *instances = kw_get_list(message, "instances", json_array(), KW_CREATE);
    json_t *active = kw_get_dict(message, "active", json_object(), KW_CREATE);

    /*---------------------------------*
     *  Apply filter of second level
     *---------------------------------*/
    /*
     *  Match fields
     */
    if(jn_record) {
        json_t *match_fields = kw_get_dict_value(
            jn_filter2,
            "match_fields",
            0,
            0
        );
        if(match_fields) {
            JSON_INCREF(match_fields);
            if(!kw_match_simple(jn_record, match_fields)) {
                JSON_DECREF(jn_record);
                return 0;  // Timeranger does not load the record, it's me.
            }
        }
    }

    /*
     *  Select fields
     */
    if(jn_record) {
        json_t *select_fields = kw_get_dict_value(
            jn_filter2,
            "select_fields",
            0,
            0
        );
        if(select_fields) {
            JSON_INCREF(select_fields);
            jn_record = kw_clone_by_keys(jn_record, select_fields, TRUE);
        }
    }

    /*
     *  Create instance
     */
    json_t *instance = json_object();
    json_t *jn_record_md = tranger_md2json(md_record);
    json_object_set_new(instance, "__md_tranger__", jn_record_md);

    json_t *user_keys = kw_get_dict_value(list, "match_cond", 0, 0); // user keys shared in all instances
    if(kw_get_bool(jn_filter2, "clone_user_keys", 0, 0)) { // Now are not shared.
        json_t *user_keys_ = user_keys;
        user_keys = json_deep_copy(user_keys);
        json_decref(user_keys_);
    }

    if(jn_record) {
        json_object_set_new(instance, "content", jn_record);
    } else {
        json_object_set_new(instance, "content", json_null());
    }

    /*
     *  Check active
     *  If tag is 0 then the last loaded msg will be the active msg
     */
    uint32_t instance_tag = md_record->__user_flag__;
    uint32_t topic_tag = kw_get_int(topic, "topic_tag", 0, 0);

    BOOL is_active = FALSE;
    if(topic_tag==0 || instance_tag == topic_tag) {
        is_active = TRUE;
    }

    /*
     *  Filter by callback
     */
// TODO    trtdb_instance_callback_t trtdb_instance_callback =
//         (trtdb_instance_callback_t)(size_t)kw_get_int(
//         list,
//         "trtdb_instance_callback",
//         0,
//         0
//     );
//     if(trtdb_instance_callback) {
//         int ret = trtdb_instance_callback(
//             tranger,
//             list,
//             is_active,
//             instance
//         );
//
//         if(ret < 0) {
//             JSON_DECREF(instance);
//             return -1;  // break the load
//         } else if(ret>0) {
//             // continue below, add the instance
//         } else { // == 0
//             JSON_DECREF(instance);
//             return 0;  // Timeranger does not load the record, it's me.
//         }
//     }

    /*
     *  max_key_instances
     */
    unsigned max_key_instances = kw_get_int(
        jn_filter2,
        "max_key_instances",
        0,
        KW_WILD_NUMBER
    );
    if(max_key_instances > 0) {
        if(json_array_size(instances) >= max_key_instances) {
            json_t *instance2remove = json_array_get(instances, 0);
            if(instance2remove != instance) {
                json_array_remove(instances, 0);
            } else {
                instance2remove = json_array_get(instances, 1);
                json_array_remove(instances, 1);
            }
            if(instance2remove == active) {
                json_object_set_new(message, "active", json_object());
            }
        }
    }

    /*
     *  Inserta
     */
    if(kw_get_bool(jn_filter2, "order_by_tm", 0, 0)) {
        /*
         *  Order by tm
         */
        json_int_t tm = kw_get_int(instance, "__md_tranger__`__tm__", 0, KW_REQUIRED);
        json_int_t last_instance = json_array_size(instances);
        json_int_t idx = last_instance;
        while(idx > 0) {
            json_t *instance_ = json_array_get(instances, idx-1);
            json_int_t tm_ = kw_get_int(instance_, "__md_tranger__`__tm__", 0, KW_REQUIRED);
            if(tm >= tm_) {
                break;
            }
            idx--;
        }
        json_array_insert_new(instances, idx, instance);

    } else {
        /*
         *  Order by rowid
         */
        json_array_append_new(instances, instance);
    }

    /*
     *  Set active
     */
    if(is_active) {
        json_object_set(message, "active", instance);
    }

    return 0;  // Timeranger does not load the record, it's me.
}

/***************************************************************************
    Being `kw` a row's list or list of dicts [{},...],
    return a new kw filtering the rows by `jn_filter` (where),
    and returning the `keys` fields of row (select).
    If match_fn is 0 then kw_match_simple is used.
 ***************************************************************************/
PRIVATE json_t *_trtdb_select(
    json_t *kw,         // not owned
    json_t *jn_fields,  // owned
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

//     if(json_is_array(kw)) {
//         size_t idx;
//         json_t *jn_value;
//         json_array_foreach(kw, idx, jn_value) {
//             KW_INCREF(jn_filter);
//             if(match_fn(jn_value, jn_filter)) {
//                 json_t *jn_row = kw_duplicate_with_only_keys(jn_value, keys);
//                 json_array_append_new(kw_new, jn_row);
//             }
//         }
//     } else if(json_is_object(kw)) {
//         KW_INCREF(jn_filter);
//         if(match_fn(kw, jn_filter)) {
//             json_t *jn_row = kw_duplicate_with_only_keys(kw, keys);
//             json_array_append_new(kw_new, jn_row);
//         }
//
//     } else  {
//         log_error(LOG_OPT_TRACE_STACK,
//             "gobj",         "%s", __FILE__,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//             "msg",          "%s", "kw MUST BE a json array or object",
//             NULL
//         );
//         return kw_new;
//     }

    KW_DECREF(jn_fields);
    KW_DECREF(jn_filter);
    return kw_new;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trtdb_select_nodes(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *filter,
    const char *options // "delete", "recursive", "verbose", "metadata"
)
{
    json_t *list = 0;
    // TODO

    return list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trtdb_link_node(
    json_t *tranger,
    json_t *kw_parent,
    json_t *kw_child,
    const char *options  // "return-child" (default), "return-parent"
)
{
    json_t *child = 0;
    // TODO

    return child;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trtdb_unlink_node(
    json_t *tranger,
    json_t *kw_parent,
    json_t *kw_child,
    const char *options  // "return-parent" (default), "return-child"
)
{
    json_t *parent = 0;
    // TODO

    return parent;
}




                        /*------------------------------------*
                         *      Transactions
                         *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int trtdb_begin_transaction(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *options
)
{
    return 0;
}

PUBLIC int trtdb_end_transaction(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *options
)
{
    return 0;
}

