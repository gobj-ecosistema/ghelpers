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
#include <uuid/uuid.h>
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
PRIVATE json_t *record2tranger(
    json_t *tranger,
    const char *topic_name,
    json_t *kw,  // not owned
    const char *options,
    BOOL create // create or update
);
PRIVATE json_t *_md2json(
    const char *treedb_name,
    const char *topic_name,
    md_record_t *md_record
);

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
        json_pack("{s:s, s:s, s:s, s:[s,s,s,s,s,s,s], s:[s,s]}",
            "id", "type",
            "header", "Type",
            "type", "enum",
            "enum",
                "string","integer","object","array","real","boolean",  "enum",
            "flag",
                "required", "notnull"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:s, s:[s,s,s,s,s,s,s,s,s], s:s}",
            "id", "flag",
            "header", "Flag",
            "type", "enum",
            "enum",
                "","persistent","volatil", "required","fkey",
                "hook","uuid","notnull","wild",
            "flag",
                ""
        )
    );
    return topic_cols_desc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *build_treedb_index_path(
    char *bf,
    int bfsize,
    const char *treedb_name,
    const char *topic_name,
    const char *key
)
{
    snprintf(bf, bfsize, "treedbs`%s`%s`%s", treedb_name, topic_name, key);
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
    json_t *jn_options
)
{
    if(!jn_schema) {
        log_error(0,
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
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "No topics found",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        JSON_DECREF(jn_schema);
        return 0;
    }

    /*
     *  The tree is built in tranger, check if already exits
     */
    json_t *treedb = kwid_get("", tranger, "treedbs`%s", treedb_name);
    if(treedb) {
        log_error(0,
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
            log_error(0,
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
            log_error(0,
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
    build_treedb_index_path(path, sizeof(path), treedb_name, tags_topic_name, "id");
    kw_get_str(list, "treedb_index_path", path, KW_CREATE);

    kw_get_subdict_value(treedb, tags_topic_name, "id", json_object(), KW_CREATE);

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

        build_treedb_index_path(path, sizeof(path), treedb_name, topic_name, "id");
        kw_get_str(list, "treedb_index_path", path, KW_CREATE);

        kw_get_subdict_value(treedb, topic_name, "id", json_object(), KW_CREATE);
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
PRIVATE int check_desc_field(json_t *desc, json_t *dato)
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
            "dato",         "%j", dato,
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
            "dato",         "%j", dato,
            NULL
        );
        return -1;
    }

    /*
     *  Get value and type
     */
    json_t *value = kw_get_dict_value(dato, desc_id, 0, 0);
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
                "dato",         "%j", dato,
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
                                    "dato",         "%j", dato,
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
                                "dato",         "%j", dato,
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
                        "dato",         "%j", dato,
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
                    "dato",         "%j", dato,
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
                    "dato",         "%j", dato,
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
    json_t *dato  // owned
)
{
    int ret = 0;

    if(!(json_is_object(dato) || json_is_array(dato))) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Data must be an {} or [{}]",
            "cols_desc",    "%j", cols_desc,
            NULL
        );
        JSON_DECREF(dato);
        return -1;
    }

    int idx; json_t *desc;

    if(json_is_object(dato)) {
        json_array_foreach(cols_desc, idx, desc) {
            ret += check_desc_field(desc, dato);
        }
    } else if(json_is_array(dato)) {
        int idx1; json_t *d;
        json_array_foreach(dato, idx1, d) {
            int idx2;
            json_array_foreach(cols_desc, idx2, desc) {
                ret += check_desc_field(desc, d);
            }
        }
    }

    JSON_DECREF(dato);

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
                /*---------------------------------*
                 *  It's a hook, check the links
                 *---------------------------------*/
                json_t *hook = kw_get_dict(col, "hook", 0, 0);
                if(!hook) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "hook field not found",
                        "topic_name",   "%s", topic_name,
                        "id",           "%s", id,
                        NULL
                    );
                    ret += -1;
                    continue;
                }

                /*----------------------------*
                 *  Check egdes
                 *----------------------------*/
                const char *link_topic_name; json_t *link_field;
                json_object_foreach(hook, link_topic_name, link_field) {
                    /*------------------------------*
                     *  Check link: who child node
                     *------------------------------*/
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
                            "link_field",       "%s", s_link_field,
                            NULL
                        );
                        ret += -1;
                    }

                    // CHEQUEA que tiene el flag fkey o que es otro hook
                    json_t *jn_flag = kwid_get("", field, "flag");
                    if(!kw_has_word(jn_flag, "fkey", "") && !kw_has_word(jn_flag, "hook", "")) {
                        log_error(0,
                            "gobj",                 "%s", __FILE__,
                            "function",             "%s", __FUNCTION__,
                            "msgset",               "%s", MSGSET_TREEDB_ERROR,
                            "msg",                  "%s", "link field must be a fkey or another hook",
                            "topic_name",       "%s", topic_name,
                            "id",               "%s", id,
                            "link_topic_name",  "%s", link_topic_name,
                            "link_field",       "%s", s_link_field,
                            NULL
                        );
                        ret += -1;
                    }  else {
                        json_object_set_new(
                            field,
                            "fkey",
                            json_pack("{s:s}",
                                topic_name, id
                            )
                        );

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
                     *      Write/Read to/from tranger
                     *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_field_value(
    const char *topic_name,
    json_t *col,    // not owned
    json_t *record, // not owned
    json_t *value,  // not owned
    BOOL create
)
{
    const char *field = kw_get_str(col, "id", 0, KW_REQUIRED);
    if(!field) {
        log_error(0,
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
        log_error(0,
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
            log_error(0,
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
            log_error(0,
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
    BOOL is_hook = kw_has_word(desc_flag, "hook", 0)?TRUE:FALSE;
    BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;

    SWITCHS(type) {
        CASES("array")
            if(is_hook) {
                json_object_set_new(record, field, json_array());
            } else if(is_fkey) {
                if(create) {
                    json_object_set_new(record, field, json_array());
                } else {
                    if(JSON_TYPEOF(value, JSON_ARRAY)) {
                        json_object_set(record, field, value);
                    } else {
                        json_object_set_new(record, field, json_array());
                    }
                }
            } else {
                if(JSON_TYPEOF(value, JSON_ARRAY)) {
                    json_object_set(record, field, value);
                } else {
                    json_object_set_new(record, field, json_array());
                }
            }
            break;

        CASES("object")
            if(is_hook) {
                json_object_set_new(record, field, json_object());
            } else if(is_fkey) {
                if(create) {
                    json_object_set_new(record, field, json_object());
                } else {
                    if(JSON_TYPEOF(value, JSON_OBJECT)) {
                        json_object_set(record, field, value);
                    } else {
                        json_object_set_new(record, field, json_object());
                    }
                }
            } else {
                if(JSON_TYPEOF(value, JSON_OBJECT)) {
                    json_object_set(record, field, value);
                } else {
                    json_object_set_new(record, field, json_object());
                }
            }
            break;

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
                log_error(0,
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
                log_error(0,
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
                log_error(0,
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

        DEFAULTS
            log_error(0,
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
PRIVATE json_t *record2tranger(
    json_t *tranger,
    const char *topic_name,
    json_t *kw,  // not owned
    const char *options,
    BOOL create // create or update
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    json_t *cols = kw_get_dict(topic, "cols", 0, 0);
    if(!cols) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return 0;
    }
    json_t *new_record = json_object();

    const char *field; json_t *col;
    json_object_foreach(cols, field, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL persistent = kw_has_word(desc_flag, "persistent", 0)?TRUE:FALSE;
        BOOL volatil = kw_has_word(desc_flag, "volatil", 0)?TRUE:FALSE;
        if(persistent || volatil) {
            if(set_field_value(
                    topic_name,
                    col,
                    new_record,
                    kw_get_dict_value(kw, field, 0, 0),
                    create
                )<0) {
                // Error already logged
                JSON_DECREF(new_record);
                JSON_DECREF(cols);
                return 0;
            }
        }
    }

    if(options && strstr(options, "permissive")) {
        json_object_update_missing(new_record, kw);
    }
    json_object_del(new_record, "__md_treedb__");

    return new_record;
}

/***************************************************************************
 *  Return json object with record metadata
 ***************************************************************************/
PRIVATE json_t *_md2json(
    const char *treedb_name,
    const char *topic_name,
    md_record_t *md_record
)
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
 *  TODO This callback para los que quiera notificaciones de escritura de topics
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
)
{
#ifdef PEPE
    char *key = md_record->key.s; // convierte las claves int a string
    char key_[64];
    if(md_record->__system_flag__ & (sf_int_key|sf_rowid_key)) {
        snprintf(key_, sizeof(key_), "%"PRIu64, md_record->key.i);
        key = key_;
    }
#endif
    JSON_DECREF(jn_record);
    return 0;  // Timeranger: does not load the record, it's me.
}




                    /*------------------------------------*
                     *      Manage the tree's nodes
                     *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_create_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw, // owned
    const char *options // "permissive"
)
{
    /*-------------------------------*
     *      Get indexx
     *-------------------------------*/
    char path[NAME_MAX];
    build_treedb_index_path(path, sizeof(path), treedb_name, topic_name, "id");
    json_t *indexx = kw_get_dict(
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    if(!indexx) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexx NOT FOUND",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(kw);
        return 0;
    }

    /*-------------------------------*
     *  Get the id, it's mandatory
     *-------------------------------*/
    json_t *new_id = 0;
    json_t *id = kw_get_dict_value(kw, "id", 0, 0);
    if(!id) {
        json_t *id_col_flag = kwid_get("verbose,lower",
            tranger,
            "topics`%s`cols`id`flag",
                topic_name
        );
        if(kw_has_word(id_col_flag, "uuid", 0)) {
            char uuid[50];
            uuid_t binuuid;
            uuid_generate_random(binuuid);
            uuid_unparse_lower(binuuid, uuid);
            new_id = id = json_string(uuid);
            json_object_set_new(kw, "id", new_id);
        } else {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Field 'id' required",
                "path",         "%s", path,
                "topic_name",   "%s", topic_name,
                "kw",           "%j", kw,
                NULL
            );
            JSON_DECREF(kw);
            return 0;
        }
    }

    /*-------------------------------*
     *  Exists already the id?
     *-------------------------------*/
    char *sid = jn2string(id);
    if(empty_string(sid)) {
        JSON_DECREF(kw);
        return 0;
    }
    json_t *record = kw_get_dict(indexx, sid, 0, 0);
    if(record) {
        /*
         *  Yes
         */
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Node already exists",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            "id",           "%s", sid,
            NULL
        );
        gbmem_free(sid);
        JSON_DECREF(kw);
        return 0;
    }

    /*-------------------------------*
     *  Create the record
     *-------------------------------*/
    record = record2tranger(tranger, topic_name, kw, options, TRUE);
    if(!record) {
        // Error already logged
        gbmem_free(sid);
        JSON_DECREF(kw);
        return 0;
    }

    /*-------------------------------*
     *  Write to tranger
     *-------------------------------*/
    md_record_t md_record;
    JSON_INCREF(record);
    int ret = tranger_append_record(
        tranger,
        topic_name,
        0, // __t__,         // if 0 then the time will be set by TimeRanger with now time
        0, // user_flag,
        &md_record, // md_record,
        record // owned
    );
    if(ret < 0) {
        // Error already logged
        gbmem_free(sid);
        JSON_DECREF(kw);
        JSON_DECREF(record);
        return 0;
    }

    /*-------------------------------*
     *  Build metadata
     *-------------------------------*/
    json_t *jn_record_md = _md2json(
        treedb_name,
        topic_name,
        &md_record
    );
    json_object_set_new(record, "__md_treedb__", jn_record_md);

    /*-------------------------------*
     *  Write node
     *-------------------------------*/
    json_object_set_new(indexx, sid, record);

    gbmem_free(sid);
    JSON_DECREF(kw);
    return record;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_update_node(
    json_t *tranger,
    json_t *node    // not owned
)
{
    /*-------------------------------*
     *      Get record info
     *-------------------------------*/
    const char *treedb_name = kw_get_str(node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(node, "__md_treedb__`topic_name", 0, KW_REQUIRED);

    /*-------------------------------*
     *  Create the record
     *-------------------------------*/
    json_t *record = record2tranger(tranger, topic_name, node, "", FALSE);
    if(!record) {
        // Error already logged
        return -1;
    }

    /*-------------------------------*
     *  Write to tranger
     *-------------------------------*/
    md_record_t md_record;
    int ret = tranger_append_record(
        tranger,
        topic_name,
        0, // __t__,         // if 0 then the time will be set by TimeRanger with now time
        0, // user_flag,
        &md_record, // md_record,
        record // owned
    );
    if(ret < 0) {
        // Error already logged
        return -1;
    }

    /*-------------------------------*
     *  Re-write metadata
     *-------------------------------*/
    json_t *jn_record_md = _md2json(
        treedb_name,
        topic_name,
        &md_record
    );
    json_object_set_new(node, "__md_treedb__", jn_record_md);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_delete_node(
    json_t *tranger,
    json_t *node    // owned
)
{
    /*-------------------------------*
     *      Get record info
     *-------------------------------*/
    const char *treedb_name = kw_get_str(node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(node, "__md_treedb__`topic_name", 0, KW_REQUIRED);
    json_int_t __rowid__ = kw_get_int(node, "__md_treedb__`__rowid__", 0, KW_REQUIRED);

    /*-------------------------------*
     *      Get indexx
     *-------------------------------*/
    char path[NAME_MAX];
    build_treedb_index_path(path, sizeof(path), treedb_name, topic_name, "id");
    json_t *indexx = kw_get_dict(
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    if(!indexx) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexx NOT FOUND",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            NULL
        );
        return -1;
    }

    /*-------------------------------*
     *  Get the id, it's mandatory
     *-------------------------------*/
    json_t *id = kw_get_dict_value(node, "id", 0, 0);
    if(!id) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Field 'id' required",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            NULL
        );
        return -1;
    }

    /*-------------------------------*
     *  Check hooks TODO
     *-------------------------------*/

    /*-------------------------------*
     *  Delete the record
     *-------------------------------*/
    if(tranger_delete_record(tranger, topic_name, __rowid__)==0) {
        char *sid = jn2string(id);
        json_object_del(indexx, sid);
        gbmem_free(sid);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_list_nodes( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_ids,     // owned
    json_t *jn_filter   // owned
)
{
    /*-------------------------------*
     *      Get indexx
     *-------------------------------*/
    char path[NAME_MAX];
    build_treedb_index_path(path, sizeof(path), treedb_name, topic_name, "id");
    json_t *indexx = kw_get_dict(
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    if(!indexx) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexx NOT FOUND",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            NULL
        );
        return 0;
    }

    /*-------------------------------*
     *      Read
     *-------------------------------*/
    json_t *list = kwid_collect(
        indexx,    // not owned
        jn_ids,     // owned
        jn_filter,  // owned
        0           // match fn
    );
    return list;

}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_get_index( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *index_name
)
{
    char path[NAME_MAX];
    build_treedb_index_path(path, sizeof(path), treedb_name, topic_name, index_name);
    json_t *indexx = kw_get_dict(
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    return indexx;

}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_get_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_id       // owned
)
{
    /*-------------------------------*
     *      Get indexx
     *-------------------------------*/
    char path[NAME_MAX];
    build_treedb_index_path(path, sizeof(path), treedb_name, topic_name, "id");
    json_t *indexx = kw_get_dict(
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    if(!indexx) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexx NOT FOUND",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_id);
        return 0;
    }

    /*-------------------------------*
     *      Get
     *-------------------------------*/
    char *sid = jn2string(jn_id);
    json_t *record = kw_get_dict(indexx, sid, 0, 0);
    gbmem_free(sid);

    JSON_DECREF(jn_id);
    return record;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_link_nodes(
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
)
{
    /*---------------------------------------*
     *  Check parent has `hook_name` field
     *---------------------------------------*/
    json_t *parent_hook_data = kw_get_dict_value(parent_node, hook_name, 0, 0);
    if(!parent_hook_data) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook field not found",
            "parent_node",  "%j", parent_node,
            "hook_name",    "%s", hook_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Get info of parent/child from their metadata
     *--------------------------------------------------*/
    const char *parent_topic_name = json_string_value(
        kwid_get("", parent_node, "__md_treedb__`topic_name")
    );
    json_t *hook_col_desc = kwid_get("verbose,lower",
        tranger,
        "topics`%s`%s`%s",
            parent_topic_name, "cols", hook_name
    );

    BOOL save_parent_node = FALSE; // Only fkeys are saved!
    //kw_has_word(kwid_get("lower", hook_col_desc, "flag"), "persistent", "");

    const char *child_topic_name = json_string_value(
        kwid_get("", child_node, "__md_treedb__`topic_name")
    );

    /*----------------------*
     *      Get hook desc
     *----------------------*/
    json_t *hook_desc = kw_get_dict(hook_col_desc, "hook", 0, 0);
    if(!hook_desc) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook field not define topic desc",
            "topic_name",   "%s", parent_topic_name,
            "hook",         "%s", hook_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Check that this child topic is in the hook desc
     *
     *  Child topic name must be defined in hook links
     *  with his child_field (fkey or hook)
     *--------------------------------------------------*/
    const char *child_field = kw_get_str(hook_desc, child_topic_name, 0, 0);
    if(!child_field) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic child not defined in hook desc links",
            "topic_name",   "%s", parent_topic_name,
            "hook",         "%s", hook_name,
            "child_topic",  "%s", child_topic_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *      Get ids
     *--------------------------------------------------*/
    const char *parent_id = kw_get_str(parent_node, "id", 0, 0);
    if(!parent_id) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Parent 'id' not found",
            "parent_topic", "%s", parent_topic_name,
            "parent",       "%j", parent_node,
            NULL
        );
        return -1;
    }
    const char *child_id = kw_get_str(child_node, "id", 0, 0);
    if(!child_id) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Child 'id' not found",
            "child_topic",  "%s", child_topic_name,
            "child",        "%j", child_node,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Reverse - child container with info of parent
     *--------------------------------------------------*/
    json_t *child_col_flag = kwid_get("verbose,lower",
        tranger,
        "topics`%s`%s`%s`flag",
            child_topic_name, "cols", child_field
    );
    if(!child_col_flag) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field child not define topic desc",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
    }

    BOOL save_child_node = FALSE;
    if(kw_has_word(child_col_flag, "persistent", "")) {
        save_child_node = TRUE;
    }

    json_t *child_data = kw_get_dict_value(child_node, child_field, 0, 0);
    if(!child_data) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field not found in the node",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
    }

    /*--------------------------------------------------*
     *      Checks
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
    case JSON_OBJECT:
        break;
    default:
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong parent hook type",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "link",                 "%s", hook_name,
            NULL
        );
        return -1;
    }

    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_STRING:
    case JSON_ARRAY:
    case JSON_OBJECT:
        break;
    default:
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong child hook type",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "child_field",          "%j", child_field,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Save child content in parent hook data
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        {
            json_array_append(parent_hook_data, child_node);
        }
        break;
    case JSON_OBJECT:
        {
            json_object_set(parent_hook_data, child_id, child_node);
        }
        break;
    default:
        break;
    }

    /*--------------------------------------------------*
     *  Save parent id child field
     *--------------------------------------------------*/
    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            json_object_set_new(child_node, child_field, json_string(parent_id));
        }
        break;
    case JSON_ARRAY:
        {
            json_array_append_new(child_data, json_string(parent_id));
        }
        break;
    case JSON_OBJECT:
        {
            json_object_set_new(child_data, parent_id, json_true());
        }
        break;
    default:
        break;
    }

    /*----------------------------*
     *      Save persistents
     *----------------------------*/
    int ret = 0;
    if(save_parent_node) {
        /*
         *  Update record
         */
        ret += treedb_update_node(tranger, parent_node);
    }

    if(save_child_node) {
        /*
         *  Update record
         */
        ret += treedb_update_node(tranger, child_node);
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_link_multiple_nodes(
    json_t *tranger,
    const char *hook,
    json_t *parent_nodes,   // not owned
    json_t *child_nodes     // not owned
)
{
    int ret = 0;
    if(json_is_object(parent_nodes) && json_is_object(child_nodes)) {
        ret = treedb_link_nodes(tranger, hook, parent_nodes, child_nodes);
    } else if(json_is_array(parent_nodes) && json_is_object(child_nodes)) {
        int idx; json_t *parent_node;
        json_array_foreach(parent_nodes, idx, parent_node) {
            ret += treedb_link_nodes(tranger, hook, parent_node, child_nodes);
        }
    } else if(json_is_object(parent_nodes) && json_is_array(child_nodes)) {
        int idx; json_t *child_node;
        json_array_foreach(child_nodes, idx, child_node) {
            ret += treedb_link_nodes(tranger, hook, parent_nodes, child_node);
        }
    } else if(json_is_array(parent_nodes) && json_is_array(child_nodes)) {
        int idx1; json_t *parent_node;
        json_array_foreach(parent_nodes, idx1, parent_node) {
            int idx2; json_t *child_node;
            json_array_foreach(child_nodes, idx2, child_node) {
                ret += treedb_link_nodes(tranger, hook, parent_node, child_node);
            }
        }
    } else {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "parent or child types must be object or array",
            "parents",      "%j", parent_nodes,
            "childs",       "%j", child_nodes,
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
    const char *hook_name,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
)
{
    /*---------------------------------------*
     *  Check parent has `hook_name` field
     *---------------------------------------*/
    json_t *parent_hook_data = kw_get_dict_value(parent_node, hook_name, 0, 0);
    if(!parent_hook_data) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook field not found",
            "parent_node",  "%j", parent_node,
            "hook_name",    "%s", hook_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Get info of parent/child from their metadata
     *--------------------------------------------------*/
    const char *parent_topic_name = json_string_value(
        kwid_get("", parent_node, "__md_treedb__`topic_name")
    );
    json_t *hook_col_desc = kwid_get("verbose,lower",
        tranger,
        "topics`%s`%s`%s",
            parent_topic_name, "cols", hook_name
    );

    BOOL save_parent_node = FALSE; // Only fkeys are saved!
    //kw_has_word(kwid_get("lower", hook_col_desc, "flag"), "persistent", "");

    const char *child_topic_name = json_string_value(
        kwid_get("", child_node, "__md_treedb__`topic_name")
    );

    /*----------------------*
     *      Get hook desc
     *----------------------*/
    json_t *hook_desc = kw_get_dict(hook_col_desc, "hook", 0, 0);
    if(!hook_desc) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook field not define topic desc",
            "topic_name",   "%s", parent_topic_name,
            "hook",         "%s", hook_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Check that this child topic is in the hook desc
     *
     *  Child topic name must be defined in hook links
     *  with his child_field (fkey or hook)
     *--------------------------------------------------*/
    const char *child_field = kw_get_str(hook_desc, child_topic_name, 0, 0);
    if(!child_field) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic child not defined in hook desc links",
            "topic_name",   "%s", parent_topic_name,
            "hook",         "%s", hook_name,
            "child_topic",  "%s", child_topic_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *      Get ids
     *--------------------------------------------------*/
    const char *parent_id = kw_get_str(parent_node, "id", 0, 0);
    if(!parent_id) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Parent 'id' not found",
            "parent_topic", "%s", parent_topic_name,
            "parent",       "%j", parent_node,
            NULL
        );
        return -1;
    }
    const char *child_id = kw_get_str(child_node, "id", 0, 0);
    if(!child_id) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Child 'id' not found",
            "child_topic",  "%s", child_topic_name,
            "child",        "%j", child_node,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Reverse - child container with info of parent
     *--------------------------------------------------*/
    json_t *child_col_flag = kwid_get("verbose,lower",
        tranger,
        "topics`%s`%s`%s`flag",
            child_topic_name, "cols", child_field
    );
    if(!child_col_flag) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field child not define topic desc",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
    }

    BOOL save_child_node = FALSE;
    if(kw_has_word(child_col_flag, "persistent", "")) {
        save_child_node = TRUE;
    }

    json_t *child_data = kw_get_dict_value(child_node, child_field, 0, 0);
    if(!child_data) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field not found in the node",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
    }

    /*--------------------------------------------------*
     *      Checks
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
    case JSON_OBJECT:
        break;
    default:
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong parent hook type",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "link",                 "%s", hook_name,
            NULL
        );
        return -1;
    }

    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_STRING:
    case JSON_ARRAY:
    case JSON_OBJECT:
        break;
    default:
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong child hook type",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "child_field",          "%j", child_field,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Save child content in parent hook data
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        {
            int idx; json_t *data;
            json_array_foreach(parent_hook_data, idx, data) {
                const char *id = kw_get_str(data, "id", 0, KW_REQUIRED);
                if(strcmp(id, child_id)==0) {
                    json_array_remove(parent_hook_data, idx);
                    break;
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            json_object_del(parent_hook_data, child_id);
        }
        break;
    default:
        break;
    }

    /*--------------------------------------------------*
     *  Save parent id child field
     *--------------------------------------------------*/
    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            json_object_set_new(child_node, child_field, json_string(""));
        }
        break;
    case JSON_ARRAY:
        {
            int idx; json_t *data;
            json_array_foreach(child_data, idx, data) {
                const char *id = kw_get_str(data, "id", 0, KW_REQUIRED);
                if(strcmp(id, parent_id)==0) {
                    json_array_remove(child_data, idx);
                    break;
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            json_object_del(child_data, parent_id);
        }
        break;
    default:
        break;
    }

    /*----------------------------*
     *      Save persistents
     *----------------------------*/
    int ret = 0;
    if(save_parent_node) {
        /*
         *  Update record
         */
        ret += treedb_update_node(tranger, parent_node);
    }

    if(save_child_node) {
        /*
         *  Update record
         */
        ret += treedb_update_node(tranger, child_node);
    }

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

