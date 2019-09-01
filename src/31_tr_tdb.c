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
PRIVATE int parse_hooks(
    json_t *tranger
);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE json_t *topic_cols_desc = 0;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *build_treedb_data_path(
    char *bf,
    int bfsize,
    const char *treedb_name,
    const char *topic_name
)
{
    snprintf(bf, bfsize, "treedbs`%s`%s`data", treedb_name, topic_name);
    strtolower(bf);
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *build_treedb_indexes_path(
    char *bf,
    int bfsize,
    const char *treedb_name,
    const char *topic_name
)
{
    snprintf(bf, bfsize, "treedbs`%s`%s`indexes", treedb_name, topic_name);
    strtolower(bf);
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_open_db( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_schema,  // owned
    const char *options
)
{
    if(!jn_schema) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB without schema.",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        return 0;
    }

    /*--------------------------------*
     *      Create desc of cols
     *--------------------------------*/
    if(!topic_cols_desc) {
        topic_cols_desc = _treedb_create_topic_cols_desc();
    } else {
        JSON_INCREF(topic_cols_desc);
    }

    /*
     *  At least 'topics' must be.
     */
    json_t *jn_schema_topics = kw_get_list(jn_schema, "topics", 0, KW_REQUIRED);
    if(!jn_schema_topics) {
        JSON_DECREF(jn_schema);
        return 0;
    }

    /*
     *  The tree is built in tranger, check if already exits
     */
    json_t *treedb = kwid_get("", tranger, "treedbs`%s", treedb_name);
    if(treedb) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB ALREADY opened.",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        JSON_DECREF(jn_schema);
        return 0;
    }

    /*-------------------------------*
     *  Open/Create "system" topics
     *-------------------------------*/
    char *tags_topic_name = "__tags__";

    json_t *tags_topic = tranger_create_topic(
        tranger,    // If topic exists then only needs (tranger,name) parameters
        tags_topic_name,
        "id",
        "",
        sf_rowid_key,
        json_pack("{s:{s:s, s:s, s:s, s:[s,s]}, s:{s:s, s:s, s:s, s:[s,s]}}",
            "id",
                "id", "id",
                "header", "Id",
                "type", "integer",
                "flag",
                    "persistent","required",

            "name",
                "id", "name",
                "header", "Name",
                "type", "string",
                "flag",
                    "persistent", "required"
        )
    );

    parse_schema_cols(
        topic_cols_desc,
        kwid_new_list("verbose", tags_topic, "cols")
    );

    /*------------------------------*
     *  Open/Create "user" topics
     *------------------------------*/
    int idx;
    json_t *schema_topic;
    json_array_foreach(jn_schema_topics, idx, schema_topic) {
        const char *topic_name = kw_get_str(schema_topic, "topic_name", "", 0);
        if(empty_string(topic_name)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Schema topic without topic_name",
                "treedb_name",  "%s", treedb_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        const char *pkey = kw_get_str(schema_topic, "pkey", "", 0);
        if(strcmp(pkey, "id")!=0) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Schema topic without pkey=id",
                "treedb_name",  "%s", treedb_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        json_t *topic = tranger_create_topic(
            tranger,    // If topic exists then only needs (tranger,name) parameters
            topic_name,
            pkey,
            kw_get_str(schema_topic, "tkey", "", 0),
            tranger_str2system_flag(kw_get_str(schema_topic, "system_flag", "", 0)),
            kwid_new_dict("verbose", schema_topic, "cols")
        );

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list("verbose", topic, "cols")
        );
    }

    /*------------------------------*
     *  Create the root of treedb
     *------------------------------*/
    json_t *treedbs = kw_get_dict(tranger, "treedbs", json_object(), KW_CREATE);
    treedb = kw_get_dict(treedbs, treedb_name, json_object(), KW_CREATE);

    /*------------------------------*
     *  Open "system" lists
     *------------------------------*/
    json_t *jn_filter = json_pack("{}");
    json_t *jn_list = json_pack("{s:s, s:o, s:I}",
        "topic_name", tags_topic_name,
        "match_cond", jn_filter,
        "load_record_callback", (json_int_t)(size_t)load_record_callback
    );
    json_t *list = tranger_open_list(
        tranger,
        jn_list // owned
    );

    kw_get_str(list, "treedb_name", treedb_name, KW_CREATE);

    char path[NAME_MAX];
    build_treedb_data_path(path, sizeof(path), treedb_name, tags_topic_name);
    kw_get_str(list, "treedb_data_path", path, KW_CREATE);
    build_treedb_indexes_path(path, sizeof(path), treedb_name, tags_topic_name);
    kw_get_str(list, "treedb_indexes_path", path, KW_CREATE);

    kw_get_subdict_value(treedb, tags_topic_name, "data", json_array(), KW_CREATE);
    kw_get_subdict_value(treedb, tags_topic_name, "indexes", json_object(), KW_CREATE);

    /*------------------------------*
     *  Open "user" lists
     *------------------------------*/
    json_array_foreach(jn_schema_topics, idx, schema_topic) {
        const char *topic_name = kw_get_str(schema_topic, "topic_name", "", 0);
        if(empty_string(topic_name)) {
            continue;
        }
        json_t *jn_filter = json_pack("{}"); // TODO pon el current tag
        json_t *jn_list = json_pack("{s:s, s:o, s:I}",
            "topic_name", topic_name,
            "match_cond", jn_filter,
            "load_record_callback", (json_int_t)(size_t)load_record_callback
        );
        list = tranger_open_list(
            tranger,
            jn_list // owned
        );

        kw_get_str(list, "treedb_name", treedb_name, KW_CREATE);

        build_treedb_data_path(path, sizeof(path), treedb_name, topic_name);
        kw_get_str(list, "treedb_data_path", path, KW_CREATE);
        build_treedb_indexes_path(path, sizeof(path), treedb_name, topic_name);
        kw_get_str(list, "treedb_indexes_path", path, KW_CREATE);

        kw_get_subdict_value(treedb, topic_name, "data", json_array(), KW_CREATE);
        kw_get_subdict_value(treedb, topic_name, "indexes", json_object(), KW_CREATE);
    }

    /*------------------------------*
     *  Parse hooks
     *------------------------------*/
    parse_hooks(tranger);

    JSON_DECREF(jn_schema);
    return treedb;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_close_db(
    json_t *tranger,
    const char *treedb_name
)
{
    json_t *treedb = kw_get_subdict_value(tranger, "treedbs", treedb_name, 0, KW_EXTRACT);
    JSON_DECREF(treedb);
    JSON_DECREF(topic_cols_desc);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_tag( // tag the current tree db
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
PUBLIC int treedb_reopen_db(
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
PUBLIC json_t *_treedb_create_topic_cols_desc(void)
{
    json_t *topic_cols_desc = json_array();
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:[s,s], s:s}",
            "id", "id",
            "header", "Id",
            "type",
                "string", "integer",
            "flag",
                "required"
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
        json_pack("{s:s, s:s, s:s, s:[s,s,s,s,s,s,s], s:[s,s,s]}",
            "id", "type",
            "header", "Type",
            "type", "enum",
            "enum",
                "string","integer","object","array","real","boolean",  "enum",
            "flag",
                "required", "notnull", "wild"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:s, s:[s,s,s,s,s,s,s], s:s}",
            "id", "flag",
            "header", "Flag",
            "type", "enum",
            "enum",
                "","persistent","required","fkey", "hook","uuid","include",
            "flag",
                ""
        )
    );
    return topic_cols_desc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_desc_field(json_t *desc, json_t *data)
{
    int ret = 0;

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
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Field 'id' required",
            "desc",         "%j", desc,
            "data",         "%j", data,
            NULL
        );
        return -1;
    }
    if(!desc_type) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Field 'type' required",
            "desc",         "%j", desc,
            "data",         "%j", data,
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
     *  Check required
     */
    if(kw_has_word(desc_flag, "required", 0)) {
        if(!value) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Field required",
                "desc",         "%j", desc,
                "data",         "%j", data,
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
        /*----------------------------*
         *      Enum
         *----------------------------*/
        CASES("enum")
            json_t *desc_enum = kw_get_list(desc, "enum", 0, 0);
            switch(json_typeof(value)) { // json_typeof NO CONTROLADO
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
                                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                                    "msg",          "%s", "Wrong enum type",
                                    "desc",         "%j", desc,
                                    "data",         "%j", data,
                                    "field",        "%s", desc_id,
                                    "value",        "%j", value,
                                    "v",            "%j", v,
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
                                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                                "msg",          "%s", "Case not implemented",
                                "desc",         "%j", desc,
                                "data",         "%j", data,
                                "field",        "%s", desc_id,
                                "value",        "%j", value,
                                "v",            "%j", v,
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
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Wrong enum type",
                        "desc",         "%j", desc,
                        "data",         "%j", data,
                        "field",        "%s", desc_id,
                        "value",        "%j", value,
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
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Enum value must be string or string's array",
                    "desc",         "%j", desc,
                    "data",         "%j", data,
                    "field",        "%s", desc_id,
                    "value",        "%j", value,
                    "value_to_be",  "%j", desc_enum,
                    NULL
                );
                ret += -1;
                break;
            }
            break;

        /*----------------------------*
         *      Json basic types
         *----------------------------*/
        DEFAULTS
            if(!kw_has_word(desc_type, value_type, 0)) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Wrong basic type",
                    "desc",         "%j", desc,
                    "data",         "%j", data,
                    "field",        "%s", desc_id,
                    "value_type",   "%s", value_type,
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
    json_t *cols_desc,  // not owned
    json_t *data  // owned
)
{
    int ret = 0;

    if(!(json_is_object(data) || json_is_array(data))) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Data must be an {} or [{}]",
            "cols_desc",    "%j", cols_desc,
            NULL
        );
        JSON_DECREF(data);
        return -1;
    }

    int idx; json_t *desc;

    if(json_is_object(data)) {
        json_array_foreach(cols_desc, idx, desc) {
            ret += check_desc_field(desc, data);
        }
    } else if(json_is_array(data)) {
        int idx1; json_t *d;
        json_array_foreach(data, idx1, d) {
            int idx2;
            json_array_foreach(cols_desc, idx2, desc) {
                ret += check_desc_field(desc, d);
            }
        }
    }

    JSON_DECREF(data);

    return ret;
}

/***************************************************************************
 *  Return 0 if ok or # of errors in negative
 ***************************************************************************/
PRIVATE int parse_hooks(
    json_t *tranger
)
{
    int ret = 0;

    json_t *topics = kw_get_dict(tranger, "topics", 0, 0);
    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        json_t *cols = kwid_new_list("verbose", topic, "cols");
        if(!cols) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "topic without cols",
                "topic_name",   "%s", topic_name,
                NULL
            );
            ret += -1;
            continue;
        }
        int idx; json_t *col;
        json_array_foreach(cols, idx, col) {
            const char *id = kw_get_str(col, "id", 0, 0);
            json_t *flag = kw_get_dict_value(col, "flag", 0, 0);
            /*-------------------------*
             *      Is a hook?
             *-------------------------*/
            if(kw_has_word(flag, "hook", 0)) {
                /*----------------------------*
                 *  It's a hook, check link
                 *----------------------------*/
                json_t *link = kw_get_dict(col, "link", 0, 0);
                if(!link) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "hook without link",
                        "topic_name",   "%s", topic_name,
                        "id",           "%s", id,
                        NULL
                    );
                    ret += -1;
                    continue;
                }
                const char *link_topic_name; json_t *link_field;
                json_object_foreach(link, link_topic_name, link_field) {
                    json_t *link_topic = kwid_get("",
                        tranger,
                        "topics`%s",
                            link_topic_name
                    );
                    if(!link_topic) {
                        log_error(0,
                            "gobj",             "%s", __FILE__,
                            "function",         "%s", __FUNCTION__,
                            "msgset",           "%s", MSGSET_TREEDB_ERROR,
                            "msg",              "%s", "link topic not found",
                            "topic_name",       "%s", topic_name,
                            "id",               "%s", id,
                            "link_topic_name",  "%s", link_topic_name,
                            NULL
                        );
                        ret += -1;
                        continue;
                    }
                    const char *s_link_field = json_string_value(link_field);
                    json_t *field = kwid_get("",
                        link_topic,
                        "cols`%s",
                            s_link_field
                    );
                    if(!field) {
                        log_error(0,
                            "gobj",             "%s", __FILE__,
                            "function",         "%s", __FUNCTION__,
                            "msgset",           "%s", MSGSET_TREEDB_ERROR,
                            "msg",              "%s", "link field not found",
                            "topic_name",       "%s", topic_name,
                            "id",               "%s", id,
                            "link_topic_name",  "%s", link_topic_name,
                            "link_field",       "%s", json_string_value(link_field),
                            NULL
                        );
                        ret += -1;
                    }
                }

                /*-------------------------*
                 *      Check reverse
                 *-------------------------*/
                json_t *reverse = kw_get_dict(col, "reverse", 0, 0);
                if(!reverse) {
                    continue;
                }
                const char *reverse_topic_name; json_t *reverse_field;
                json_object_foreach(reverse, reverse_topic_name, reverse_field) {
                    json_t *reverse_topic = kwid_get("",
                        tranger,
                        "topics`%s",
                            reverse_topic_name
                    );
                    if(!reverse_topic) {
                        log_error(0,
                            "gobj",                 "%s", __FILE__,
                            "function",             "%s", __FUNCTION__,
                            "msgset",               "%s", MSGSET_TREEDB_ERROR,
                            "msg",                  "%s", "reverse topic not found",
                            "topic_name",           "%s", topic_name,
                            "id",                   "%s", id,
                            "reverse_topic_name",   "%s", reverse_topic_name,
                            NULL
                        );
                        ret += -1;
                        continue;
                    }

                    json_t *field = kwid_get("",
                        reverse_topic,
                        "cols`%s",
                            json_string_value(reverse_field)
                    );
                    if(!field) {
                        log_error(0,
                            "gobj",                 "%s", __FILE__,
                            "function",             "%s", __FUNCTION__,
                            "msgset",               "%s", MSGSET_TREEDB_ERROR,
                            "msg",                  "%s", "reverse field not found",
                            "topic_name",           "%s", topic_name,
                            "id",                   "%s", id,
                            "reverse_topic_name",   "%s", reverse_topic_name,
                            "reverse_field",        "%s", json_string_value(reverse_field),
                            NULL
                        );
                        ret += -1;
                    }
                    // CHEQUEA que tiene el flag fkey
                    if(!kw_has_word(kwid_get("", field, "flag"), "fkey", "")) {
                        log_error(0,
                            "gobj",                 "%s", __FILE__,
                            "function",             "%s", __FUNCTION__,
                            "msgset",               "%s", MSGSET_TREEDB_ERROR,
                            "msg",                  "%s", "reverse field must have fkey flag",
                            "topic_name",           "%s", topic_name,
                            "id",                   "%s", id,
                            "reverse_topic_name",   "%s", reverse_topic_name,
                            "reverse_field",        "%s", json_string_value(reverse_field),
                            NULL
                        );
                        ret += -1;
                    }
                }
            } else {
                /*---------------------------------*
                 *  It's not a hook,
                 *  check it hasn't link/reverse
                 *---------------------------------*/
                if(kw_has_key(col, "link")) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Not hook with a link",
                        "topic_name",   "%s", topic_name,
                        "id",           "%s", id,
                        NULL
                    );
                    ret += -1;
                }
                if(kw_has_key(col, "reverse")) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Not hook with a reverse",
                        "topic_name",   "%s", topic_name,
                        "id",           "%s", id,
                        NULL
                    );
                    ret += -1;
                }
            }
        }
        JSON_DECREF(cols);
    }

    return ret;
}




                    /*------------------------------------*
                     *      Manage the tree's nodes
                     *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_read_node( // Working with explicit 'id' returns a dict, without returns a list
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id,     // owned, Explicit id. Can be: integer,string, [integer], [string], [keys]
    json_t *kw,     // owned, HACK kw is `filter` on read/delete and `record` on create
    const char *options // "create", TODO "delete",
)
{
    /*-------------------------------*
     *      Get data and indexes
     *-------------------------------*/
    char path[NAME_MAX];
    build_treedb_data_path(path, sizeof(path), treedb_name, topic_name);
    json_t *data = kw_get_list(
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    build_treedb_indexes_path(path, sizeof(path), treedb_name, topic_name);
    json_t *indexes = kw_get_dict(
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    if(!data) {
        log_error(
            LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic data NOT FOUND",
            "path",         "%s", path,
            NULL
        );
        JSON_DECREF(id);
        JSON_DECREF(kw);
        return 0;
    }
    if(!indexes) {
        log_error(
            LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexes NOT FOUND",
            "path",         "%s", path,
            NULL
        );
        JSON_DECREF(id);
        JSON_DECREF(kw);
        return 0;
    }

    if(id) {
        /*-------------------------------*
         *      Working with id
         *  NOTE Must return a dict
         *-------------------------------*/
        char *id_ = jn2string(id);
        json_t *record = kw_get_dict(indexes, id_, 0, 0);
        gbmem_free(id_);
        if(record) {
            /*
             *  Found
             */
            JSON_DECREF(id);
            JSON_DECREF(kw);
            JSON_INCREF(record);
            return record;
        }
        /*
         *  Not found, create if option
         */
        if(!(options && strstr(options, "create"))) {
            // No create
            JSON_DECREF(id);
            JSON_DECREF(kw);
            return 0;
        }
        JSON_INCREF(id);
        JSON_INCREF(kw);
        int ret = treedb_write_node(
            tranger,
            treedb_name,
            topic_name,
            id, // owned
            kw, // owned
            options
        );
        if(ret < 0) {
            JSON_DECREF(id);
            JSON_DECREF(kw);
            return 0;
        }
        return treedb_read_node(
            tranger,
            treedb_name,
            topic_name,
            id,     // owned
            kw,     // owned
            options
        );
    } else {
        /*-------------------------------*
         *      Working without id
         *  NOTE Must return a list
         *-------------------------------*/
        JSON_INCREF(kw);
        json_t *record = kw_collect(
            data,   // not owned
            kw,     // owned, filter HACK use kw as filter
            0       // match fn
        );
        if(json_array_size(record)>0) {
            /*
             *  Found
             */
            JSON_DECREF(kw);
            return record;
        }
        JSON_DECREF(record);

        /*
         *  Not found, create if option
         */
        if(!(options && strstr(options, "create"))) {
            JSON_DECREF(kw);
            return 0;
        }

        /*
         *  Not found, create if option
         */
        JSON_INCREF(kw);
        int ret = treedb_write_node(
            tranger,
            treedb_name,
            topic_name,
            0,
            kw, // owned
            options
        );
        if(ret < 0) {
            JSON_DECREF(kw);
            return json_array();
        }
        return treedb_read_node(
            tranger,
            treedb_name,
            topic_name,
            0,      // owned
            kw,     // owned
            options
        );
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_field_value(
    const char *topic_name,
    json_t *col,    // not owned
    json_t *record, // not owned
    json_t *value  // not owned
)
{
    const char *field = kw_get_str(col, "id", 0, KW_REQUIRED);
    if(!field) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Col desc without 'id'",
            "topic_name",   "%s", topic_name,
            "col",          "%j", col,
            NULL
        );
        return -1;
    }
    const char *type = kw_get_str(col, "type", 0, KW_REQUIRED);
    if(!type) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Col desc without 'type'",
            "topic_name",   "%s", topic_name,
            "col",          "%j", col,
            "field",        "%s", field,
            NULL
        );
        return -1;
    }
    json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);

    /*
     *  Required
     */
    if(kw_has_word(desc_flag, "required", 0)) {
        if(!value || json_is_null(value)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Field required",
                "topic_name",   "%s", topic_name,
                "field",        "%s", field,
                "value",        "%j", value,
                NULL
            );
            return -1;
        }
    }

    /*
     *  Null
     */
    if(json_is_null(value)) {
        if(kw_has_word(desc_flag, "notnull", 0)) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Field cannot be null",
                "topic_name",   "%s", topic_name,
                "field",        "%s", field,
                "value",        "%j", value,
                NULL
            );
            return -1;
        } else {
            json_object_set(record, field, value);
            return 0;
        }
    }

    BOOL wild_conversion = kw_has_word(desc_flag, "wild", 0)?TRUE:FALSE;
    BOOL persistent = kw_has_word(desc_flag, "persistent", 0)?TRUE:FALSE;

    SWITCHS(type) {
        CASES("string")
            if(!value) {
                json_object_set_new(record, field, json_string(""));
            } else if(json_is_string(value)) {
                json_object_set(record, field, value);
            } else if(wild_conversion) {
                char *v = jn2string(value);
                json_object_set_new(record, field, json_string(v));
                gbmem_free(v);
            } else {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Value must be string",
                    "topic_name",   "%s", topic_name,
                    "col",          "%j", col,
                    "field",        "%s", field,
                    "value",        "%j", value,
                    NULL
                );
                return -1;
            }
            break;

        CASES("integer")
            if(!value) {
                json_object_set_new(record, field, json_integer(0));
            } else if(json_is_integer(value)) {
                json_object_set(record, field, value);
            } else if(wild_conversion) {
                json_int_t v = jn2integer(value);
                json_object_set_new(record, field, json_integer(v));
            } else {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Value must be integer",
                    "topic_name",   "%s", topic_name,
                    "col",          "%j", col,
                    "field",        "%s", field,
                    "value",        "%j", value,
                    NULL
                );
                return -1;
            }
            break;
        CASES("real")
            if(!value) {
                json_object_set_new(record, field, json_real(0.0));
            } else if(json_is_real(value)) {
                json_object_set(record, field, value);
            } else if(wild_conversion) {
                double v = jn2real(value);
                json_object_set_new(record, field, json_real(v));
            } else {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Value must be real",
                    "topic_name",   "%s", topic_name,
                    "col",          "%j", col,
                    "field",        "%s", field,
                    "value",        "%j", value,
                    NULL
                );
                return -1;
            }
            break;

        CASES("boolean")
            BOOL v = jn2bool(value);
            json_object_set_new(record, field, v?json_true():json_false());
            break;

        CASES("array")
            if(JSON_TYPEOF(value, JSON_ARRAY) && persistent) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_array());
            }
            break;

        CASES("object")
            if(JSON_TYPEOF(value, JSON_OBJECT) && persistent) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_object());
            }
            break;

        DEFAULTS
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Col type unknown",
                "topic_name",   "%s", topic_name,
                "col",          "%j", col,
                "field",        "%s", field,
                "type",         "%s", type,
                NULL
            );
            return -1;
    } SWITCHS_END;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *create_new_record(
    json_t *topic,  // not owned
    json_t *id, // not owned
    json_t *kw,  // not owned
    const char *options
)
{
    const char *topic_name = kw_get_str(topic, "topic_name", "", KW_REQUIRED);
    json_t *cols = kwid_new_list("verbose", topic, "cols");
    if(!cols) {
        return 0;
    }
    json_t *new_record = json_object();

    // TODO check cols id with uuid

    int idx; json_t *col;
    json_array_foreach(cols, idx, col) {
        const char *field = kw_get_str(col, "id", 0, 0);
        if(strcmp(field, "id")==0) {
            if(id) {
                /*
                 *  Explicit id
                 */
                if(set_field_value(topic_name, col, new_record, id)<0) {
                    // Error already logged
                    JSON_DECREF(new_record);
                    JSON_DECREF(cols);
                    return 0;
                }
                continue;
            }
        }

        if(set_field_value(topic_name, col, new_record, kw_get_dict_value(kw, field, 0, 0))<0) {
            // Error already logged
            JSON_DECREF(new_record);
            JSON_DECREF(cols);
            return 0;
        }
    }

    json_object_update_missing(new_record, kw);
    json_object_del(new_record, "__md_treedb__");

    JSON_DECREF(cols);

    return new_record;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_write_node(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id, // owned
    json_t *kw, // owned
    const char *options // "strict"
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    json_t *new_record = create_new_record(topic, id, kw, options);
    if(!new_record) {
        // Error already logged
        JSON_DECREF(new_record);
        JSON_DECREF(id);
        JSON_DECREF(kw);
        return -1;
    }

    /*
     *  Write record
     */
    md_record_t md_record;
    int ret = tranger_append_record(
        tranger,
        topic_name,
        0, // __t__,         // if 0 then the time will be set by TimeRanger with now time
        0, // user_flag,
        &md_record, // md_record,
        new_record // owned
    );

    JSON_DECREF(id);
    JSON_DECREF(kw);
    return ret;
}

/***************************************************************************
 *  Return json object with record metadata
 ***************************************************************************/
PRIVATE json_t *_md2json(const char *treedb_name, const char *topic_name, md_record_t *md_record)
{
    json_t *jn_md = json_object();
    json_object_set_new(
        jn_md,
        "treedb_name",
        json_string(treedb_name)
    );
    json_object_set_new(
        jn_md,
        "topic_name",
        json_string(topic_name)
    );
    json_object_set_new(jn_md, "__rowid__", json_integer(md_record->__rowid__));
    json_object_set_new(jn_md, "__t__", json_integer(md_record->__t__));
    json_object_set_new(jn_md, "__tag__", json_integer(md_record->__user_flag__));

    return jn_md;
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
    char *key = md_record->key.s; // convierte las claves int a string
    char key_[64];
    if(md_record->__system_flag__ & (sf_int_key|sf_rowid_key)) {
        snprintf(key_, sizeof(key_), "%"PRIu64, md_record->key.i);
        key = key_;
    }

    /*
     *  Search the record of this key
     */
    /*-------------------------------*
     *      Get data and indexes
     *-------------------------------*/
    json_t *data = kw_get_list(
        tranger,
        kw_get_str(list, "treedb_data_path", 0, KW_REQUIRED),
        0,
        KW_REQUIRED
    );
    json_t *indexes = kw_get_dict(
        tranger,
        kw_get_str(list, "treedb_indexes_path", 0, KW_REQUIRED),
        0,
        KW_REQUIRED
    );
    if(!data || !indexes) {
        JSON_DECREF(jn_record);
        return -1;  // Timeranger: break the load
    }

    /*
     *  Build metadata
     */
    json_t *jn_record_md = _md2json(
        kw_get_str(list, "treedb_name", "", KW_REQUIRED),
        kw_get_str(list, "topic_name", "", KW_REQUIRED),
        md_record
    );

    /*
     *  Exists already the id?
     */
    json_t *record = kw_get_dict(indexes, key, 0, 0);
    if(!record) {
        /*
         *  New record
         */
        json_object_set_new(jn_record, "__md_treedb__", jn_record_md);
        json_object_set(indexes, key, jn_record);
        json_array_append_new(data, jn_record);
        return 0;  // Timeranger does not load the record, it's me.
    }

    /*
     *  Update record
     */
    json_object_clear(record);
    json_object_update(record, jn_record);
    json_object_set_new(jn_record, "__md_treedb__", jn_record_md);

    JSON_DECREF(jn_record);
    return 0;  // Timeranger: does not load the record, it's me.
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int link_node(
    json_t *tranger,
    const char *treedb_name,
    const char *link_,
    json_t *parent_record, // not owned
    json_t *child_record,  // not owned
    const char *options     //
)
{
    json_t *parent_hook = kw_get_dict_value(parent_record, link_, 0, 0);
    if(!parent_hook) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook field not found",
            "parent_record","%j", parent_record,
            "hook field",   "%s", link_,
            NULL
        );
        return -1;
    }

    const char *parent_topic_name = json_string_value(
        kwid_get("", parent_record, "__md_treedb__`topic_name")
    );
    json_t *hook_col = kwid_get("verbose,lower",
        tranger,
        "topics`%s`%s`%s",
            parent_topic_name, "cols", link_
    );
    const char *child_topic_name = json_string_value(
        kwid_get("", child_record, "__md_treedb__`topic_name")
    );

    /*-----------------------------------------------*
     *  Link - parent container with id of child
     *-----------------------------------------------*/
    json_t *link = kw_get_dict(hook_col, "link", 0, 0);
    if(!link) {
        log_error(
            LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "link field not found",
            "topic_name",   "%s", parent_topic_name,
            "link",         "%s", link_,
            NULL
        );
        return -1;
    }
    BOOL link_found = FALSE;
    const char *link_topic_name; json_t *link_field_;
    json_object_foreach(link, link_topic_name, link_field_) {
        if(strcmp(child_topic_name, link_topic_name)==0) {
            const char *link_field = json_string_value(link_field_);
            const char *id  = kw_get_str(child_record, link_field, 0, 0); // TODO y si es integer?
            switch(json_typeof(parent_hook)) { // json_typeof PROTECTED
                case JSON_STRING:
                    {
                        if(json_object_set_new(parent_record, link_, json_string(id))==0) {
                            link_found = TRUE;
                        }
                    }
                    break;
                case JSON_ARRAY:
                    {
                        if(json_array_append(parent_hook, child_record)==0) {
                            link_found = TRUE;
                        }
                    }
                    break;
                case JSON_OBJECT:
                    {
                        if(json_object_set(parent_hook, id, child_record)==0) {
                            link_found = TRUE;
                        }
                    }
                    break;
                default:
                    log_error(
                        LOG_OPT_TRACE_STACK,
                        "gobj",                 "%s", __FILE__,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "wrong parent hook type",
                        "parent_topic_name",    "%s", parent_topic_name,
                        "child_topic_name",     "%s", child_topic_name,
                        "link",                 "%s", link_,
                        "parent_hook",          "%j", parent_hook,
                        NULL
                    );
                    return -1;
            }
        }
    }
    if(!link_found) {
        log_error(
            LOG_OPT_TRACE_STACK,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "link to child failed",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "link",                 "%s", link_,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Reverse - child container with info of parent
     *--------------------------------------------------*/
    BOOL reverse_found = FALSE;
    json_t *reverse = kw_get_dict(hook_col, "reverse", 0, 0);
    if(reverse) {
        const char *reverse_topic_name; json_t *reverse_field_;
        json_object_foreach(reverse, reverse_topic_name, reverse_field_) {
            if(strcmp(parent_topic_name, reverse_topic_name)==0) {
                const char *reverse_field = json_string_value(reverse_field_);
                json_t *child_field = kw_get_dict_value(child_record, reverse_field, 0, 0);
                if(!child_field) {
                    break;
                }
                const char *id  = kw_get_str(parent_record, "id", 0, 0); // TODO y si es integer?
                switch(json_typeof(child_field)) { // json_typeof PROTECTED
                    case JSON_STRING:
                        {
                            if(json_object_set_new(child_record, reverse_field, json_string(id))==0) {
                                reverse_found = TRUE;
                            }
                        }
                        break;
                    case JSON_ARRAY:
                        {
                            if(json_array_append_new(child_field, json_string(id))==0) {
                                reverse_found = TRUE;
                            }
                        }
                        break;
                    case JSON_OBJECT:
                        {
                            if(json_object_set(child_field, id, parent_record)==0) {
                                reverse_found = TRUE;
                            }
                        }
                        break;
                    default:
                        log_error(
                            LOG_OPT_TRACE_STACK,
                            "gobj",                 "%s", __FILE__,
                            "function",             "%s", __FUNCTION__,
                            "msgset",               "%s", MSGSET_TREEDB_ERROR,
                            "msg",                  "%s", "wrong child hook type",
                            "parent_topic_name",    "%s", parent_topic_name,
                            "child_topic_name",     "%s", child_topic_name,
                            "link",                 "%s", link_,
                            "child_field",          "%j", child_field,
                            NULL
                        );
                        return -1;
                }
            }
        }
        if(!reverse_found) {
            log_error(
                LOG_OPT_TRACE_STACK,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                "msg",                  "%s", "link to child failed",
                "parent_topic_name",    "%s", parent_topic_name,
                "child_topic_name",     "%s", child_topic_name,
                "link",                 "%s", link_,
                NULL
            );
            return -1;
        }
    }

    /*----------------------------*
     *      Save persistents
     *----------------------------*/
    int ret = 0;
    if(link_found) {
        /*
         *  Write record
         */
        JSON_INCREF(parent_record);
        ret += treedb_write_node(
            tranger,
            treedb_name,
            parent_topic_name,
            0,              // id: owned, Explicit id. Can be: integer,string
            parent_record,  // owned
            options
        );
    }

    if(reverse_found) {
        /*
         *  Write record
         */
        JSON_INCREF(child_record);
        ret += treedb_write_node(
            tranger,
            treedb_name,
            child_topic_name,
            0,              // id
            child_record,   // owned
            options
        );
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_link_nodes(
    json_t *tranger,
    const char *treedb_name,
    const char *link,
    json_t *parent_records, // not owned
    json_t *child_records,  // not owned
    const char *options     //
)
{
    int ret = 0;
    if(json_is_object(parent_records) && json_is_object(child_records)) {
        ret = link_node(tranger, treedb_name, link, parent_records, child_records, options);
    } else if(json_is_array(parent_records) && json_is_object(child_records)) {
        int idx; json_t *parent_record;
        json_array_foreach(parent_records, idx, parent_record) {
            ret += link_node(tranger, treedb_name, link, parent_record, child_records, options);
        }
    } else if(json_is_object(parent_records) && json_is_array(child_records)) {
        int idx; json_t *child_record;
        json_array_foreach(child_records, idx, child_record) {
            ret += link_node(tranger, treedb_name, link, parent_records, child_record, options);
        }
    } else if(json_is_array(parent_records) && json_is_array(child_records)) {
        int idx1; json_t *parent_record;
        json_array_foreach(parent_records, idx1, parent_record) {
            int idx2; json_t *child_record;
            json_array_foreach(child_records, idx2, child_record) {
                ret += link_node(tranger, treedb_name, link, parent_record, child_record, options);
            }
        }
    } else {
        log_error(
            LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "parent or child types must be object or array",
            "parents",      "%j", parent_records,
            "childs",       "%j", child_records,
            NULL
        );
        return -1;
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_unlink_node(
    json_t *tranger,
    const char *treedb_name,
    const char *link_,
    json_t *parent_records, // not owned
    json_t *child_records,  // not owned
    const char *options     //
)
{
    int ret = 0;

    //TODO

    return ret;
}




                        /*------------------------------------*
                         *      Transactions
                         *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_begin_transaction(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *options
)
{
    // TODO
    return 0;
}

PUBLIC int treedb_end_transaction(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *options
)
{
    // TODO
    return 0;
}

