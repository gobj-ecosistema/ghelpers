/***********************************************************************
 *          TR_TREEDB.C
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
#include "31_tr_treedb.h"

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

PRIVATE int load_hook_links(
    json_t *tranger,
    const char *treedb_name
);
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
);

/**rst**
    Return a list of hook field names of the topic.
**rst**/
PRIVATE json_t *tranger_hook_names( // Return MUST be decref
    json_t *topic_desc // owned
);

/**rst**
    Return a view of node with hook fields being collapsed
**rst**/
PRIVATE json_t *tranger_collapsed_view( // Return MUST be decref
    json_t *jn_hook_names, // not owned
    json_t *node // not owned
);

PRIVATE json_t *get_hook_refs(
    json_t *hook_data // not owned
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
        json_pack("{s:s, s:s, s:s, s:[s,s,s,s,s,s,s,s], s:s}",
            "id", "flag",
            "header", "Flag",
            "type", "enum",
            "enum",
                "","persistent","required","fkey",
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
PUBLIC char *build_treedb_index_path(
    char *bf,
    int bfsize,
    const char *treedb_name,
    const char *topic_name,
    const char *key
)
{
    snprintf(bf, bfsize, "treedbs`%s`%s`%s", treedb_name, topic_name, key);
    return bf;
}

/***************************************************************************
    Open tree db (Remember previously open tranger_startup())

    Tree of nodes of data. Node's data contains attributes, in json.

    The nodes are linked with relation parent->child
    in pure-child or multi-parent mode.
    The tree in json too.

    HACK Conventions:
        1) the pkey of all topics must be "id".
        2) the "id" field (primary key) MUST be a string.

    Option "persistent"
        Try to load the schema from file
        File has precedence.
        Once saved,
            if you want to change the schema
            then you must remove the file
 ***************************************************************************/
PUBLIC json_t *treedb_open_db( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
)
{
    if(empty_string(treedb_name)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "treedb_name NULL",
            NULL
        );
        JSON_DECREF(jn_schema);
        return 0;
    }

    char schema_filename[NAME_MAX];
    if(strstr(treedb_name, ".treedb_schema.json")) {
        snprintf(schema_filename, sizeof(schema_filename), "%s",
            treedb_name
        );
    } else {
        snprintf(schema_filename, sizeof(schema_filename), "%s.treedb_schema.json",
            treedb_name
        );
    }

    char schema_full_path[NAME_MAX];
    snprintf(schema_full_path, sizeof(schema_full_path), "%s/%s",
        kw_get_str(tranger, "directory", "", KW_REQUIRED),
        schema_filename
    );

    if(options && strstr(options,"persistent")) {
        if(file_exists(schema_full_path, 0)) {
            JSON_DECREF(jn_schema);
            jn_schema = load_json_from_file(
                schema_full_path,
                "",
                kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED)
            );
        } else if(jn_schema) {
            JSON_INCREF(jn_schema);
            save_json_to_file(
                kw_get_str(tranger, "directory", 0, KW_REQUIRED),
                schema_filename,
                kw_get_int(tranger, "xpermission", 0, KW_REQUIRED),
                kw_get_int(tranger, "rpermission", 0, KW_REQUIRED),
                kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
                TRUE, // Create file if not exists or overwrite.
                FALSE, // only_read
                jn_schema     // owned
            );
        }
        if(!jn_schema) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot load TreeDB schema from file.",
                "treedb_name",  "%s", treedb_name,
                "schema_file",  "%s", schema_full_path,
                NULL
            );
            return 0;
        }
    } else if(!jn_schema) {
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
    char path[NAME_MAX];
    build_treedb_index_path(path, sizeof(path), treedb_name, tags_topic_name, "id");

    kw_get_subdict_value(treedb, tags_topic_name, "id", json_object(), KW_CREATE);

    json_t *jn_filter = json_pack("{s:b}",
        "backward", 1
    );
    json_t *jn_list = json_pack("{s:s, s:s, s:o, s:I, s:s, s:{}}",
        "id", path,
        "topic_name", tags_topic_name,
        "match_cond", jn_filter,
        "load_record_callback", (json_int_t)(size_t)load_record_callback,
        "treedb_name", treedb_name,
        "deleted_records"
    );
    tranger_open_list(
        tranger,
        jn_list // owned
    );


    /*------------------------------*
     *  Open "user" lists
     *------------------------------*/
    json_array_foreach(jn_schema_topics, idx, schema_topic) {
        const char *topic_name = kw_get_str(schema_topic, "topic_name", "", 0);
        if(empty_string(topic_name)) {
            continue;
        }
        build_treedb_index_path(path, sizeof(path), treedb_name, topic_name, "id");

        kw_get_subdict_value(treedb, topic_name, "id", json_object(), KW_CREATE);

        json_t *jn_filter = json_pack("{s:b}", // TODO pon el current tag
            "backward", 1
        );
        json_t *jn_list = json_pack("{s:s, s:s, s:o, s:I, s:s, s:{}}",
            "id", path,
            "topic_name", topic_name,
            "match_cond", jn_filter,
            "load_record_callback", (json_int_t)(size_t)load_record_callback,
            "treedb_name", treedb_name,
            "deleted_records"
        );
        tranger_open_list(
            tranger,
            jn_list // owned
        );
    }

    /*------------------------------*
     *  Parse hooks
     *------------------------------*/
    parse_hooks(tranger);

    /*------------------------------*
     *  Load hook links
     *------------------------------*/
    load_hook_links(tranger, treedb_name);

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
    /*------------------------------*
     *  Close treedb lists
     *------------------------------*/
    json_t *treedb = kw_get_subdict_value(tranger, "treedbs", treedb_name, 0, KW_EXTRACT);
    if(!treedb) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB not found.",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        return -1;
    }

    char list_id[NAME_MAX];
    const char *topic_name; json_t *topic_records;
    json_object_foreach(treedb, topic_name, topic_records) {
        build_treedb_index_path(list_id, sizeof(list_id), treedb_name, topic_name, "id");
        json_t *list = tranger_get_list(tranger, list_id);
        if(!list) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "List not found.",
                "treedb_name",  "%s", treedb_name,
                "list",         "%s", list_id,
                NULL
            );
            continue;
        }
        tranger_close_list(tranger, list);
    }

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
PUBLIC int parse_hooks(
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

                    // CHEQUEA que tiene el flag fkey
                    json_t *jn_flag = kwid_get("", field, "flag");
                    if(!kw_has_word(jn_flag, "fkey", "")) {
                        log_error(0,
                            "gobj",             "%s", __FILE__,
                            "function",         "%s", __FUNCTION__,
                            "msgset",           "%s", MSGSET_TREEDB_ERROR,
                            "msg",              "%s", "link field must be a fkey",
                            "topic_name",       "%s", topic_name,
                            "id",               "%s", id,
                            "link_topic_name",  "%s", link_topic_name,
                            "link_field",       "%s", s_link_field,
                            NULL
                        );
                        ret += -1;
                        continue;
                    }

                    if(kw_has_word(field, "fkey", "")) {
                        log_error(0,
                            "gobj",             "%s", __FILE__,
                            "function",         "%s", __FUNCTION__,
                            "msgset",           "%s", MSGSET_TREEDB_ERROR,
                            "msg",              "%s", "Only can be one fkey",
                            "topic_name",       "%s", topic_name,
                            "id",               "%s", id,
                            "link_topic_name",  "%s", link_topic_name,
                            "link_field",       "%s", s_link_field,
                            NULL
                        );
                        ret += -1;
                        continue;
                    }

                    json_object_set_new(
                        field,
                        "fkey",
                        json_pack("{s:s}",
                            topic_name, id
                        )
                    );
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
PRIVATE json_t *filtra_fkeys(json_t *value)
{
    json_t *mix_ids = 0;

    switch(json_typeof(value)) {
    case JSON_ARRAY:
        {
            int idx; json_t *v;
            json_array_foreach(value, idx, v) {
                if(json_typeof(v)==JSON_STRING) {
                    const char *id = json_string_value(v);
                    if(count_char(id, '^')==2) {
                        if(!mix_ids) {
                            mix_ids = json_array();
                        }
                        json_array_append_new(mix_ids, json_string(id));
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *id; json_t *v;
            json_object_foreach(value, id, v) {
                if(count_char(id, '^')==2) {
                    if(!mix_ids) {
                        mix_ids = json_object();
                    }
                    json_object_set_new(mix_ids, id, json_true());
                }
            }
        }
        break;
    case JSON_STRING:
        {
            const char *v = json_string_value(value);
            if(count_char(v, '^')==2) {
                mix_ids = json_string(v);
            }
        }
        break;
    default:
        break;
    }

    return mix_ids;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_tranger_field_value(
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

    BOOL is_persistent = kw_has_word(desc_flag, "persistent", 0)?TRUE:FALSE;
    BOOL wild_conversion = kw_has_word(desc_flag, "wild", 0)?TRUE:FALSE;
    BOOL is_hook = kw_has_word(desc_flag, "hook", 0)?TRUE:FALSE;
    BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
    if(!(is_persistent || is_hook || is_fkey)) {
        // Not save to tranger
        return 0;
    }

    SWITCHS(type) {
        CASES("array")
            if(is_fkey || is_hook) {
                if(create) {
                    json_object_set_new(record, field, json_array());
                } else {
                    json_t *mix_ids = 0;
                    if(is_fkey && (mix_ids=filtra_fkeys(value))) {
                        json_object_set_new(record, field, mix_ids);
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
            if(is_fkey || is_hook) {
                if(create) {
                    // No dejes poner datos en la creación.
                    json_object_set_new(record, field, json_object());
                } else {
                    json_t *mix_ids = 0;
                    if(is_fkey && (mix_ids=filtra_fkeys(value))) {
                        json_object_set_new(record, field, mix_ids);
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
    json_t *cols = kwid_new_dict("", topic, "cols");
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
        json_t *value = kw_get_dict_value(kw, field, 0, 0);
        if(set_tranger_field_value(
                topic_name,
                col,
                new_record,
                value,
                create
            )<0) {
            // Error already logged
            JSON_DECREF(new_record);
            JSON_DECREF(cols);
            return 0;
        }
    }

    if(options && strstr(options, "permissive")) {
        json_object_update_missing(new_record, kw);
    }
    json_object_del(new_record, "__md_treedb__");

    JSON_DECREF(cols);
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
 *  When record is loaded from disk then create the node
 *  when is loaded from memory then notify to subscribers
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
)
{
    json_t *deleted_records = kw_get_dict(list, "deleted_records", 0, KW_REQUIRED);

    if(md_record->__system_flag__ & (sf_loading_from_disk)) {
        /*---------------------------------*
         *  Loading treedb from disk
         *---------------------------------*/
        if(md_record->__system_flag__ & (sf_mark1)) {
            /*---------------------------------*
             *      Record deleted
             *---------------------------------*/
            json_object_set_new(
                deleted_records,
                md_record->key.s,
                json_true()
            );
        } else {
            /*-------------------------------------*
             *  If not deleted record append node
             *-------------------------------------*/
            if(!json_object_get(deleted_records, md_record->key.s)) {
                /*-------------------------------*
                 *      Get indexx
                 *-------------------------------*/
                const char *treedb_name = kw_get_str(list, "treedb_name", 0, KW_REQUIRED);
                const char *topic_name = kw_get_str(list, "topic_name", 0, KW_REQUIRED);

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
                    JSON_DECREF(jn_record);
                    return 0;  // Timeranger: does not load the record, it's mine.
                }

                /*-------------------------------*
                 *  Exists already the node?
                 *-------------------------------*/
                if(kw_get_dict(
                        indexx,
                        md_record->key.s,
                        0,
                        0
                    )!=0) {
                    // The node with this key already exists
                } else {
                    /*-------------------------------*
                     *  Append new node
                     *-------------------------------*/
                    /*-------------------------------*
                     *  Build metadata
                     *-------------------------------*/
                    json_t *jn_record_md = _md2json(
                        treedb_name,
                        topic_name,
                        md_record
                    );
                    json_object_set_new(jn_record_md, "__pending_links__", json_true());
                    json_object_set_new(jn_record, "__md_treedb__", jn_record_md);

                    /*-------------------------------*
                     *  Write node
                     *-------------------------------*/
                    json_object_set(
                        indexx,
                        md_record->key.s,
                        jn_record
                    );
                }
            }
        }
    } else {
        /*---------------------------------*
         *      Working in memory
         *---------------------------------*/
        if(json_object_get(deleted_records, md_record->key.s)) {
            // This key is operative again
            json_object_del(deleted_records, md_record->key.s);
        }
    }

    JSON_DECREF(jn_record);
    return 0;  // Timeranger: does not load the record, it's mine.
}

/***************************************************************************
 *  Link hooks
 ***************************************************************************/
PRIVATE int load_hook_links(
    json_t *tranger,
    const char *treedb_name
)
{
    int ret = 0;

    /*
     *  Loop topics, as child nodes
     */
    json_t *topics = kw_get_dict(tranger, "topics", 0, KW_REQUIRED);
    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        /*
         *  Loop nodes searching links
         */
        char path[NAME_MAX];
        build_treedb_index_path(path, sizeof(path), treedb_name, topic_name, "id");
        json_t *indexx = kw_get_dict(
            tranger,
            path,
            0,
            KW_REQUIRED
        );

        const char *child_id; json_t *child_node;
        json_object_foreach(indexx, child_id, child_node) {
            json_t *__md_treedb__ = kw_get_dict(child_node, "__md_treedb__", 0, KW_REQUIRED);
            BOOL __pending_links__ = kw_get_bool(
                __md_treedb__,
                "__pending_links__",
                0,
                KW_EXTRACT
            );
            if(!__pending_links__) {
                continue;
            }

            /*
             *  Loop desc cols searching fkey
             */
            json_t *cols = kwid_new_dict("", topic, "cols");
            const char *col_name; json_t *col;
            json_object_foreach(cols, col_name, col) {
                json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
                BOOL is_child_hook = kw_has_word(desc_flag, "hook", "")?TRUE:FALSE;
                BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
                if(!is_fkey) {
                    continue;
                }

                json_t *child_data = kw_get_dict_value(child_node, col_name, 0, 0);
                if(!child_data) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "field not found in the node",
                        "topic_name",   "%s", topic_name,
                        "field",        "%s", col_name,
                        NULL
                    );
                }

                json_t *fkey = kwid_get("", col, "fkey");
                if(!fkey) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "Child node without fkey field",
                        "treedb_name",  "%s", treedb_name,
                        "topic_name",   "%s", topic_name,
                        "col_name",     "%s", col_name,
                        "record",       "%j", child_node,
                        NULL
                    );
                    ret += -1;
                    continue;
                }

                /*
                 *  Check fkey, the field that contains hook parent info.
                 */
                const char *parent_topic_name; json_t *jn_parent_field_name;
                json_object_foreach(fkey, parent_topic_name, jn_parent_field_name) {
                    const char *hook_name = json_string_value(jn_parent_field_name);
                    /*
                     *  Get ids from child_node fkey field
                     */
                    json_t *ids = kwid_get_new_ids(kw_get_dict_value(child_node, col_name, 0, 0));
                    int ids_idx; json_t *jn_mix_id;
                    json_array_foreach(ids, ids_idx, jn_mix_id) {
                        /*
                         *  Find the parent node
                         */
                        const char *pref = json_string_value(jn_mix_id);
                        if(empty_string((pref)) || !strchr(pref, '^')) {
                            continue;
                        }
                        int list_size;
                        const char **ss = split2(pref, "^", &list_size);
                        if(list_size!=3) {
                            log_error(0,
                                "gobj",         "%s", __FILE__,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "Wrong mix fkey",
                                "treedb_name",  "%s", treedb_name,
                                "topic_name",   "%s", topic_name,
                                "col_name",     "%s", col_name,
                                "mix_id",       "%j", jn_mix_id,
                                "record",       "%j", child_node,
                                NULL
                            );
                            ret += -1;
                            split_free2(ss);
                            continue;
                        }

                        if(strcmp(parent_topic_name, ss[0])!=0) {
                            split_free2(ss);
                            continue;
                        }
                        const char *parent_id = ss[1];
                        json_t *parent_node = treedb_get_node( // Return is NOT YOURS
                            tranger,
                            treedb_name,
                            parent_topic_name,
                            parent_id
                        );
                        if(!parent_node) {
                            log_error(0,
                                "gobj",         "%s", __FILE__,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "Node not found",
                                "treedb_name",  "%s", treedb_name,
                                "topic_name",   "%s", parent_topic_name,
                                "col_name",     "%s", col_name,
                                "id",           "%s", parent_id,
                                NULL
                            );
                            ret += -1;
                            split_free2(ss);
                            continue;
                        }
                        const char *hook_name_ = 0;
                        hook_name_ = ss[2];
                        if(strcmp(hook_name, hook_name_)!=0) {
                            log_error(0,
                                "gobj",         "%s", __FILE__,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "Hook name not match",
                                "treedb_name",  "%s", treedb_name,
                                "topic_name",   "%s", parent_topic_name,
                                "col_name",     "%s", col_name,
                                "id",           "%s", parent_id,
                                "hook_name1",   "%s", hook_name,
                                "hook_name2",   "%s", hook_name_,
                                NULL
                            );
                            ret += -1;
                            split_free2(ss);
                            continue;
                        }

                        /*--------------------------------------------------*
                         *  In parent hook data: save child content
                         *  WARNING repeated in
                         *      treedb_link_nodes() and load_hook_links()
                         *--------------------------------------------------*/
                        json_t *parent_hook_data = kw_get_dict_value(parent_node, hook_name, 0, 0);
                        if(!parent_hook_data) {
                            log_error(0,
                                "gobj",         "%s", __FILE__,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                                "msg",          "%s", "hook field not found",
                                "parent_node",  "%j", parent_node,
                                "hook_name",    "%s", hook_name,
                                "col_name",     "%s", col_name,
                                NULL
                            );
                            split_free2(ss);
                            continue;
                        }
                        switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
                        case JSON_ARRAY:
                            {
                                if(is_child_hook) {
                                    json_array_append(parent_hook_data, child_data);

                                } else {
                                    json_array_append(parent_hook_data, child_node);
                                }
                            }
                            break;
                        case JSON_OBJECT:
                            {
                                char pref[NAME_MAX];
                                if(is_child_hook) {
                                    snprintf(pref, sizeof(pref), "%s~%s~%s",
                                        topic_name,
                                        child_id,
                                        col_name
                                    );
                                    json_object_set(parent_hook_data, pref, child_data);
                                } else {
                                    json_object_set(parent_hook_data, child_id, child_node);
                                }
                            }
                            break;
                        default:
                            log_error(0,
                                "gobj",                 "%s", __FILE__,
                                "function",             "%s", __FUNCTION__,
                                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                                "msg",                  "%s", "wrong parent hook type",
                                "parent_topic_name",    "%s", parent_topic_name,
                                "col_name",             "%s", col_name,
                                "link",                 "%s", hook_name,
                                NULL
                            );
                            break;
                        }
                        split_free2(ss);
                    }
                    JSON_DECREF(ids);
                }
            }
            JSON_DECREF(cols);
        }
    }

    return ret;
}

/***************************************************************************
    Being `ids` a:

        {
            "$id": {
                "id": "$id",
                ...
            }
            ...
        }

        [
            {
                "id":$id,
                ...
            },
            ...
        ]

    return a new list of all node's mix_id
 ***************************************************************************/
PRIVATE json_t *get_hook_refs(
    json_t *hook_data // not owned
)
{
    char mix_id[NAME_MAX];
    json_t *refs = json_array();

    switch(json_typeof(hook_data)) {
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
            json_object_foreach(hook_data, id, jn_value) {
                int ampersands = count_char(id, '^');
                int tildes = count_char(id, '~');

                if(tildes== 2) {
                    json_array_append_new(refs, json_string(id));
                    continue;
                } else if(ampersands == 2) {
                    continue;
                }

                const char *topic_name = kw_get_str(
                    jn_value,
                    "__md_treedb__`topic_name",
                    0,
                    0
                );
                if(!topic_name) {
                    log_error(0,
                        "gobj",                 "%s", __FILE__,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "__md_treedb__ not found",
                        NULL
                    );
                    log_debug_json(0, hook_data, "hook_data: __md_treedb__ not found");
                    log_debug_json(0, jn_value, "jn_value: __md_treedb__ not found");
                    continue;
                }
                snprintf(mix_id, sizeof(mix_id), "%s^%s", topic_name, id);
                json_array_append_new(refs, json_string(mix_id));
            }
        }
        break;

    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(hook_data, idx, jn_value) {
                switch(json_typeof(jn_value)) {
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
                        const char *id = kw_get_str(jn_value, "id", 0, 0);
                        if(id) {
                            const char *topic_name = kw_get_str(
                                jn_value,
                                "__md_treedb__`topic_name",
                                0,
                                0
                            );
                            if(!topic_name) {
                                log_error(0,
                                    "gobj",                 "%s", __FILE__,
                                    "function",             "%s", __FUNCTION__,
                                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                                    "msg",                  "%s", "__md_treedb__ not found",
                                    NULL
                                );
                                log_debug_json(0, hook_data, "hook_data: __md_treedb__ not found");
                                log_debug_json(0, jn_value, "jn_value: __md_treedb__ not found");
                                break;
                            }
                            snprintf(mix_id, sizeof(mix_id), "%s^%s", topic_name, id);
                            json_array_append_new(refs, json_string(mix_id));
                        }
                    }
                    break;
                case JSON_STRING:
                    {
                        int ampersands = count_char(json_string_value(jn_value), '^');
                        int tildes = count_char(json_string_value(jn_value), '~');


                        if(tildes== 2) {
                            json_array_append(refs, jn_value);
                            break;
                        } else if(ampersands == 2) {
                            break;
                        }
                    }
                default:
                    log_error(0,
                        "gobj",                 "%s", __FILE__,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "wrong array child hook type",
                        "jn_value",             "%j", jn_value,
                        "hook_data",            "%j", hook_data,
                        NULL
                    );
                    break;
                }
            }
        }
        break;

    default:
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong child hook type",
            "hook_data",            "%j", hook_data,
            NULL
        );
        break;
    }

    return refs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_fkey_refs(
    json_t *field_data // not owned
)
{
    json_t *refs = json_array();

    switch(json_typeof(field_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            json_array_append(refs, field_data);
        }
        break;
    case JSON_ARRAY:
        {
            int idx; json_t *ref;
            json_array_foreach(field_data, idx, ref) {
                if(json_typeof(ref)==JSON_STRING) {
                    if(count_char(json_string_value(ref), '^')==2) {
                        json_array_append(refs, ref);
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *ref; json_t *v;
            json_object_foreach(field_data, ref, v) {
                if(count_char(ref, '^')==2) {
                    json_array_append_new(refs, json_string(ref));
                }
            }
        }
        break;
    default:
        break;
    }

    return refs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL json_empty(json_t *value)
{
    if(!value) {
        return TRUE;
    }
    switch(json_typeof(value)) {
    case JSON_ARRAY:
        return json_array_size(value)==0?TRUE:FALSE;
    case JSON_OBJECT:
        return json_object_size(value)==0?TRUE:FALSE;
    case JSON_STRING:
        return strlen(json_string_value(value))==0?TRUE:FALSE;
    default:
        return TRUE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_down_refs(  // Return MUST be decref
    json_t *tranger,
    json_t *node    // not owned
)
{
    json_t *refs = json_array();

    const char *treedb_name = kw_get_str(node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(node, "__md_treedb__`topic_name", 0, KW_REQUIRED);
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_hook = kw_has_word(desc_flag, "hook", "")?TRUE:FALSE;
        if(!is_hook) {
            continue;
        }

        json_t *field_data = kw_get_dict_value(node, col_name, 0, 0);
        if(!field_data) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "field not found in the node",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                "field",        "%s", col_name,
                NULL
            );
        }
        if(json_empty(field_data)) {
            continue;
        }

        json_t *ref = get_hook_refs(field_data);
        json_array_extend(refs, ref);
        JSON_DECREF(ref);
    }

    JSON_DECREF(cols);
    return refs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_up_refs(  // Return MUST be decref
    json_t *tranger,
    json_t *node    // not owned
)
{
    json_t *refs = json_array();

    const char *treedb_name = kw_get_str(node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(node, "__md_treedb__`topic_name", 0, KW_REQUIRED);
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
        BOOL is_hook = kw_has_word(desc_flag, "hook", "")?TRUE:FALSE;
        if(!is_fkey && !is_hook) {
            continue;
        }

        json_t *field_data = kw_get_dict_value(node, col_name, 0, 0);
        if(!field_data) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "field not found in the node",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                "field",        "%s", col_name,
                NULL
            );
        }

        if(json_empty(field_data)) {
            continue;
        }

        json_t *ref = get_fkey_refs(field_data);
        json_array_extend(refs, ref);
        JSON_DECREF(ref);
    }

    JSON_DECREF(cols);
    return refs;
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
    const char *options // "permissive" "verbose"
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
    char uuid[RECORD_KEY_VALUE_MAX+1];
    const char *id = kw_get_str(kw, "id", 0, 0);
    if(empty_string(id)) {
        json_t *id_col_flag = kwid_get("verbose",
            tranger,
            "topics`%s`cols`id`flag",
                topic_name
        );
        if(kw_has_word(id_col_flag, "uuid", 0)) {
            uuid_t binuuid;
            uuid_generate_random(binuuid);
            uuid_unparse_lower(binuuid, uuid);
            id = uuid;
            json_object_set_new(kw, "id", json_string(id));
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
    json_t *record = kw_get_dict(indexx, id, 0, 0);
    if(record) {
        /*
         *  Yes
         */
        if(strstr(options, "verbose")) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Node already exists",
                "path",         "%s", path,
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
        }
        JSON_DECREF(kw);
        return 0;
    }

    /*----------------------------------------*
     *  Create the tranger record to create
     *----------------------------------------*/
    record = record2tranger(tranger, topic_name, kw, options, TRUE);
    if(!record) {
        // Error already logged
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
    json_object_set_new(indexx, id, record);

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

    /*---------------------------------------*
     *  Create the tranger record to update
     *---------------------------------------*/
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
    const char *id = kw_get_str(node, "id", 0, 0);
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
     *  Check hooks
     *-------------------------------*/
    BOOL to_delete = TRUE;
    json_t *up_refs = get_up_refs(tranger, node);
    if(json_array_size(up_refs)>0) {
        to_delete = FALSE;
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot delete node: has up links",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
    }
    JSON_DECREF(up_refs);

    json_t *down_refs = get_down_refs(tranger, node);
    if(json_array_size(down_refs)>0) {
        to_delete = FALSE;
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot delete node: has down links",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
    }
    JSON_DECREF(down_refs);

    if(!to_delete) {
        // Error already logged
        return -1;
    }

    /*-------------------------------*
     *  Delete the record
     *-------------------------------*/
    if(tranger_write_mark1(tranger, topic_name, __rowid__, TRUE)==0) {
        json_object_del(indexx, id);
    } else {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot mark node as deleted",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 * Return a list of hook field names of the topic.
 * Return MUST be decref
 ***************************************************************************/
PUBLIC json_t *tranger_hook_names(
    json_t *topic_desc // owned
)
{
    json_t *jn_hook_field_names = json_array();

    int idx; json_t *col;
    json_array_foreach(topic_desc, idx, col) {
        json_t *flag = kw_get_dict_value(col, "flag", 0, 0);
        /*-------------------------*
         *      Is a hook?
         *-------------------------*/
        if(kw_has_word(flag, "hook", 0)) {
            json_array_append(jn_hook_field_names, json_object_get(col, "id"));
        }
    }
    JSON_DECREF(topic_desc);
    return jn_hook_field_names;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger_list_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    return kwid_new_list("verbose", topic, "cols");
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger_dict_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    return kwid_new_dict("verbose", topic, "cols");
}

/***************************************************************************
    Return a view of node with hook fields being collapsed
 ***************************************************************************/
PUBLIC json_t *tranger_collapsed_view( // Return MUST be decref
    json_t *jn_hook_names, // not owned
    json_t *node // not owned
)
{
    json_t *node_view = json_object();

    const char *field_name; json_t *field_value;
    json_object_foreach(node, field_name, field_value) {
        if(json_str_in_list(jn_hook_names, field_name, 0)) {
            json_t *list = kw_get_dict_value(
                node_view,
                field_name,
                json_array(),
                KW_CREATE
            );
            json_t *hook_refs = get_hook_refs(field_value);
            json_array_extend(list, hook_refs);
            json_decref(hook_refs);

        } else {
            json_object_set_new(node_view, field_name, json_deep_copy(field_value));
        }
    }
    return node_view;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_list_nodes( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_ids,     // owned
    json_t *jn_filter,  // owned
    json_t *jn_options, // owned "expand"
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
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
        JSON_DECREF(jn_ids);
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options);
        return 0;
    }

    /*-------------------------------*
     *      Read
     *-------------------------------*/

    if(!match_fn) {
        match_fn = kw_match_simple;
    }

    BOOL expand = kw_get_bool(jn_options, "expand", 0, KW_WILD_NUMBER);
    json_t *hook_names = tranger_hook_names(
        tranger_list_topic_desc(tranger, topic_name)
    );

    json_t *list = json_array();

    if(json_is_array(indexx)) {
        size_t idx;
        json_t *node;
        json_array_foreach(indexx, idx, node) {
            if(!kwid_match_id(jn_ids, kw_get_str(node, "id", 0, 0))) {
                continue;
            }
            JSON_INCREF(jn_filter);
            if(match_fn(node, jn_filter)) {
                if(expand) {
                    // Full tree
                    json_array_append(list, node);
                } else {
                    // Collapse records (hook fields)
                    json_array_append_new(
                        list,
                        tranger_collapsed_view(
                            hook_names,
                            node
                        )
                    );
                }
            }
        }
    } else if(json_is_object(indexx)) {
        const char *id; json_t *node;
        json_object_foreach(indexx, id, node) {
            if(!kwid_match_id(jn_ids, id)) {
                continue;
            }
            JSON_INCREF(jn_filter);
            if(match_fn(node, jn_filter)) {
                if(expand) {
                    json_array_append(list, node);
                } else {
                    json_array_append_new(
                        list,
                        tranger_collapsed_view(
                            hook_names,
                            node
                        )
                    );
                }
            }
        }

    } else  {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw MUST BE a json array or object",
            NULL
        );
        JSON_DECREF(list);
    }

    json_decref(hook_names);
    JSON_DECREF(jn_ids);
    JSON_DECREF(jn_filter);
    JSON_DECREF(jn_options);

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
    const char *id
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
     *      Get
     *-------------------------------*/
    json_t *record = kw_get_dict(indexx, id, 0, 0);
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
    json_t *hook_col_desc = kwid_get("verbose",
        tranger,
        "topics`%s`%s`%s",
            parent_topic_name, "cols", hook_name
    );

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
    json_t *child_col_flag = kwid_get("verbose",
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

    BOOL is_child_hook = kw_has_word(child_col_flag, "hook", "")?TRUE:FALSE;

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
        break;
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
    case JSON_ARRAY:
        break;
    case JSON_OBJECT:
        break;
    case JSON_STRING:
        if(!is_child_hook) {
            break;
        }
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
     *  In parent hook data: save child content
     *  WARNING repeated in
     *      treedb_link_nodes() and load_hook_links()
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        {
            if(is_child_hook) {
                json_array_append(parent_hook_data, child_data);

            } else {
                json_array_append(parent_hook_data, child_node);
            }
        }
        break;
    case JSON_OBJECT:
        {
            char pref[NAME_MAX];
            if(is_child_hook) {
                snprintf(pref, sizeof(pref), "%s~%s~%s",
                    child_topic_name,
                    child_id,
                    child_field
                );
                json_object_set(parent_hook_data, pref, child_data);
            } else {
                json_object_set(parent_hook_data, child_id, child_node);
            }
        }
        break;
    default:
        break;
    }

    /*--------------------------------------------------*
     *  In child field: save parent ref
     *--------------------------------------------------*/
    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );

            json_object_set_new(
                child_node,
                child_field,
                json_string(pref)
            );
        }
        break;
    case JSON_ARRAY:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );
            json_array_append_new(
                child_data,
                json_string(pref)
            );
        }
        break;
    case JSON_OBJECT:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );

            json_object_set_new(
                child_data,
                pref,
                json_true()
            );
        }
        break;
    default:
        break;
    }

    /*----------------------------*
     *      Save persistents
     *  Only childs are saved
     *----------------------------*/
    return  treedb_update_node(tranger, child_node);
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
PUBLIC int treedb_unlink_nodes(
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
    json_t *hook_col_desc = kwid_get("verbose",
        tranger,
        "topics`%s`%s`%s",
            parent_topic_name, "cols", hook_name
    );

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
    json_t *child_col_flag = kwid_get("verbose",
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

    BOOL is_child_hook = FALSE;
    if(kw_has_word(child_col_flag, "hook", "")) {
        is_child_hook = TRUE;
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
        break;
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
    case JSON_ARRAY:
        break;
    case JSON_OBJECT:
        break;
    case JSON_STRING:
        if(!is_child_hook) {
            break;
        }
    default:
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong child hook type",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "child_field",          "%s", child_field,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  In parent hook data: save child content
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        {
            if(is_child_hook) {
                // link: json_array_append(parent_hook_data, child_data);
                BOOL found = FALSE;
                int idx; json_t *data;
                json_array_foreach(parent_hook_data, idx, data) {
                    if(child_data == data) {
                        json_array_remove(parent_hook_data, idx);
                        found = TRUE;
                        break;
                    }
                }
                if(!found) {
                    log_error(0,
                        "gobj",                 "%s", __FILE__,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "Child data not found in list parent hook",
                        "parent_topic_name",    "%s", parent_topic_name,
                        "hook_name",            "%s", hook_name,
                        "parent_id",            "%s", parent_id,
                        "child_topic_name",     "%s", child_topic_name,
                        "child_id",             "%s", child_id,
                        "child_field",          "%s", child_field,
                        NULL
                    );
                }
            } else {
                // link: json_array_append(parent_hook_data, child_node);
                BOOL found = FALSE;
                int idx; json_t *data;
                json_array_foreach(parent_hook_data, idx, data) {
                    if(child_node == data) {
                        json_array_remove(parent_hook_data, idx);
                        found = TRUE;
                        break;
                    }
                }
                if(!found) {
                    log_error(0,
                        "gobj",                 "%s", __FILE__,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "Child data not found in dict parent hook",
                        "parent_topic_name",    "%s", parent_topic_name,
                        "hook_name",            "%s", hook_name,
                        "parent_id",            "%s", parent_id,
                        "child_topic_name",     "%s", child_topic_name,
                        "child_id",             "%s", child_id,
                        "child_field",          "%s", child_field,
                        NULL
                    );
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            if(is_child_hook) {
                char pref[NAME_MAX];
                snprintf(pref, sizeof(pref), "%s~%s~%s",
                    child_topic_name,
                    child_id,
                    child_field
                );
                json_object_del(parent_hook_data, pref);
            } else {
                json_object_del(parent_hook_data, child_id);
            }
        }
        break;
    default:
        break;
    }

    /*--------------------------------------------------*
     *  In child field: save parent ref
     *--------------------------------------------------*/
    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            json_object_set_new(child_node, child_field, json_string(""));
        }
        break;
    case JSON_ARRAY:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );
            BOOL found = FALSE;
            int idx; json_t *data;
            json_array_foreach(child_data, idx, data) {
                if(json_typeof(data)==JSON_STRING) {
                    if(strcmp(pref, json_string_value(data))==0) {
                        json_array_remove(child_data, idx);
                        found = TRUE;
                        break;
                    }
                }
            }
            if(!found) {
                log_error(0,
                    "gobj",                 "%s", __FILE__,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "Parent ref not found in array child data",
                    "parent_topic_name",    "%s", parent_topic_name,
                    "hook_name",            "%s", hook_name,
                    "parent_id",            "%s", parent_id,
                    "child_topic_name",     "%s", child_topic_name,
                    "child_id",             "%s", child_id,
                    "child_field",          "%s", child_field,
                    NULL
                );
            }
        }
        break;
    case JSON_OBJECT:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );
            if(kw_has_key(child_data, pref)) {
                json_object_del(child_data, pref);
            } else {
                log_error(0,
                    "gobj",                 "%s", __FILE__,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "Parent ref not found in dict child data",
                    "parent_topic_name",    "%s", parent_topic_name,
                    "hook_name",            "%s", hook_name,
                    "parent_id",            "%s", parent_id,
                    "child_topic_name",     "%s", child_topic_name,
                    "child_id",             "%s", child_id,
                    "child_field",          "%s", child_field,
                    NULL
                );
            }
        }
        break;

    default:
        break;
    }

    /*----------------------------*
     *      Save persistents
     *  Only childs are saved
     *----------------------------*/
    return treedb_update_node(tranger, child_node);
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

