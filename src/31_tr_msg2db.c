/***********************************************************************
 *          TR_MSG2.C
 *
 *          Messages (ordered by pkey,pkey2: active and their instances) with TimeRanger
 *
 *          Double dict of messages (message: active and instances)
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 *
    Load in memory a iter of topic's **active** messages ordered by a sub-key,
    with n 'instances' of each key.
    If topic tag is 0:
        the active message of each key series is the **last** key instance.
    else:
        the active message is the **last** key instance with tag equal to topic tag.

***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "31_tr_msg2db.h"

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

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE json_t *topic_cols_desc = 0;

/***************************************************************************
    Open a message2 db (Remember previously open tranger_startup())

    List of pkey/pkey2 of nodes of data.
    Node's data contains attributes, in json (record, node, message, ... what you want)


    HACK Conventions:
        1) the pkey of all topics must be "id".
        2) the "id" field (primary key) MUST be a string.
        3) define a second index `pkey2`,

    Option "persistent"
        Try to load the schema from file
        File has precedence.
        Once saved,
            if you want to change the schema
            then you must remove the file
 ***************************************************************************/
PUBLIC json_t *msg2db_open_db(
    json_t *tranger,
    const char *msg2db_name,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
)
{
    if(empty_string(msg2db_name)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "msg2db_name NULL",
            NULL
        );
        JSON_DECREF(jn_schema);
        return 0;
    }

    char schema_filename[NAME_MAX];
    if(strstr(msg2db_name, ".msg2db_schema.json")) {
        snprintf(schema_filename, sizeof(schema_filename), "%s",
            msg2db_name
        );
    } else {
        snprintf(schema_filename, sizeof(schema_filename), "%s.msg2db_schema.json",
            msg2db_name
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
            log_info(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Creating TreeDB schema file.",
                "msg2db_name",  "%s", msg2db_name,
                "schema_file",  "%s", schema_full_path,
                NULL
            );
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
                "msg2db_name",  "%s", msg2db_name,
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
            "msg2db_name",  "%s", msg2db_name,
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
            "msg2db_name",  "%s", msg2db_name,
            NULL
        );
        JSON_DECREF(jn_schema);
        return 0;
    }

    /*
     *  The tree is built in tranger, check if already exits
     */
    json_t *msg2db = kwid_get("", tranger, "msg2dbs`%s", msg2db_name);
    if(msg2db) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB ALREADY opened.",
            "msg2db_name",  "%s", msg2db_name,
            NULL
        );
        JSON_DECREF(jn_schema);
        return 0;
    }

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
                "msg2db_name",  "%s", msg2db_name,
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
                "msg2db_name",  "%s", msg2db_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        const char *pkey2 = kw_get_str(schema_topic, "pkey2", "", 0);
        if(empty_string(pkey2)) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Schema topic without pkey2=id",
                "msg2db_name",  "%s", msg2db_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        json_t *jn_topic_var = json_object();
        json_object_set_new(jn_topic_var, "pkey2", json_string(pkey2));
        json_t *topic = tranger_create_topic(
            tranger,    // If topic exists then only needs (tranger,name) parameters
            topic_name,
            pkey,
            kw_get_str(schema_topic, "tkey", "", 0),
            tranger_str2system_flag(kw_get_str(schema_topic, "system_flag", "", 0)),
            kwid_new_dict("verbose", schema_topic, "cols"),
            jn_topic_var
        );

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list("verbose", topic, "cols")
        );
    }

    /*------------------------------*
     *  Create the root of msg2db
     *------------------------------*/
    json_t *msg2dbs = kw_get_dict(tranger, "msg2dbs", json_object(), KW_CREATE);
    msg2db = kw_get_dict(msg2dbs, msg2db_name, json_object(), KW_CREATE);

    /*------------------------------*
     *  Open "system" lists
     *------------------------------*/
    char path[NAME_MAX];

    /*------------------------------*
     *  Open "user" lists
     *------------------------------*/
    json_array_foreach(jn_schema_topics, idx, schema_topic) {
        const char *topic_name = kw_get_str(schema_topic, "topic_name", "", 0);
        if(empty_string(topic_name)) {
            continue;
        }
        build_treedb_index_path(path, sizeof(path), msg2db_name, topic_name, "id");

        kw_get_subdict_value(msg2db, topic_name, "id", json_object(), KW_CREATE);

        json_t *jn_filter = json_pack("{s:b}", // TODO pon el current tag
            "backward", 1
        );
        json_t *jn_list = json_pack("{s:s, s:s, s:o, s:I, s:s, s:{}}",
            "id", path,
            "topic_name", topic_name,
            "match_cond", jn_filter,
            "load_record_callback", (json_int_t)(size_t)load_record_callback,
            "msg2db_name", msg2db_name,
            "deleted_records"
        );
        tranger_open_list(
            tranger,
            jn_list // owned
        );
    }

    JSON_DECREF(jn_schema);
    return msg2db;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int msg2db_close_db(
    json_t *tranger,
    const char *msg2db_name
)
{
    /*------------------------------*
     *  Close msg2db lists
     *------------------------------*/
    json_t *msg2db = kw_get_subdict_value(tranger, "msg2dbs", msg2db_name, 0, KW_EXTRACT);
    if(!msg2db) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB not found.",
            "msg2db_name",  "%s", msg2db_name,
            NULL
        );
        return -1;
    }

    char list_id[NAME_MAX];
    const char *topic_name; json_t *topic_records;
    json_object_foreach(msg2db, topic_name, topic_records) {
        build_treedb_index_path(list_id, sizeof(list_id), msg2db_name, topic_name, "id");
        json_t *list = tranger_get_list(tranger, list_id);
        if(!list) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "List not found.",
                "msg2db_name",  "%s", msg2db_name,
                "list",         "%s", list_id,
                NULL
            );
            continue;
        }
        tranger_close_list(tranger, list);
    }

    JSON_DECREF(msg2db);
    JSON_DECREF(topic_cols_desc);
    return 0;
}





                    /*------------------------------------*
                     *      Write/Read to/from tranger
                     *------------------------------------*/




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
            if(create) {
                json_object_set_new(record, field, json_array());
            } else {
                if(JSON_TYPEOF(value, JSON_ARRAY)) {
                    json_object_set(record, field, value);
                } else {
                    json_object_set_new(record, field, json_array());
                }
            }
            break;

        CASES("object")
            if(create) {
                // No dejes poner datos en la creación.
                json_object_set_new(record, field, json_object());
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
PRIVATE int set_volatil_field_value(
    const char *type,
    const char *field,
    json_t *record, // not owned
    json_t *value   // not owned
)
{
    SWITCHS(type) {
        CASES("array")
            if(JSON_TYPEOF(value, JSON_ARRAY)) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_array());
            }
            break;

        CASES("object")
            if(JSON_TYPEOF(value, JSON_OBJECT)) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_object());
            }
            break;

        CASES("string")
            if(!value) {
                json_object_set_new(record, field, json_string(""));
            } else if(json_is_string(value)) {
                json_object_set(record, field, value);
            } else {
                char *v = jn2string(value);
                json_object_set_new(record, field, json_string(v));
                gbmem_free(v);
            }
            break;

        CASES("integer")
            if(!value) {
                json_object_set_new(record, field, json_integer(0));
            } else if(json_is_integer(value)) {
                json_object_set(record, field, value);
            } else {
                json_int_t v = jn2integer(value);
                json_object_set_new(record, field, json_integer(v));
            }
            break;
        CASES("real")
            if(!value) {
                json_object_set_new(record, field, json_real(0.0));
            } else if(json_is_real(value)) {
                json_object_set(record, field, value);
            } else {
                double v = jn2real(value);
                json_object_set_new(record, field, json_real(v));
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
PRIVATE int set_volatil_values(
    json_t *tranger,
    const char *topic_name,
    json_t *record,  // not owned
    json_t *kw // not owned
)
{
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);
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

    const char *field; json_t *col;
    json_object_foreach(cols, field, col) {
        json_t *value = kw_get_dict_value(kw, field, 0, 0);

        const char *field = kw_get_str(col, "id", 0, KW_REQUIRED);
        if(!field) {
            continue;
        }
        const char *type = kw_get_str(col, "type", 0, KW_REQUIRED);
        if(!type) {
            continue;
        }
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_persistent = kw_has_word(desc_flag, "persistent", 0)?TRUE:FALSE;
        BOOL is_hook = kw_has_word(desc_flag, "hook", 0)?TRUE:FALSE;
        BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
        if((is_persistent || is_hook || is_fkey)) {
            continue;
        }

        set_volatil_field_value(
            type,
            field,
            record, // not owned
            value   // not owned
        );
    }

    JSON_DECREF(cols);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *record2tranger(
    json_t *tranger,
    const char *topic_name,
    json_t *kw,  // not owned
    const char *options, // "permissive"
    BOOL create // create or update
)
{
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);
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
    json_object_set_new(jn_md, "__original_node__", json_true());

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
                    /*---------------------------------------------*
                     *  Build metadata, loading node from tranger
                     *---------------------------------------------*/
                    json_t *jn_record_md = _md2json(
                        treedb_name,
                        topic_name,
                        md_record
                    );
                    json_object_set_new(jn_record, "__md_treedb__", jn_record_md);

                    /*--------------------------------------------*
                     *  Set volatil data
                     *--------------------------------------------*/
                    set_volatil_values(
                        tranger,
                        topic_name,
                        jn_record,  // not owned
                        jn_record // not owned
                    );

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


PUBLIC int msg2db_save_message(
    json_t *tranger,
    json_t *message    // not owned
)
{
}

/**rst**
    "create" ["permissive"] create message if not exist
    "clean" clean wrong fkeys
**rst**/
PUBLIC json_t *msg2db_update_message( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *kw,    // owned
    const char *options // "create" ["permissive"], "clean"
)
{
}

PUBLIC json_t *msg2db_list_messages( // Return MUST be decref
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *jn_ids,     // owned
    json_t *jn_filter,  // owned
    json_t *jn_options, // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
}

PUBLIC json_t *msg2db_get_message( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id,
    const char *id2
)
{
}
PUBLIC json_t *msg2db_get_message_active( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id,
    const char *id2
)
{
}
PUBLIC json_t *msg2db_get_message_instances( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id,
    const char *id2
)
{
}

/***************************************************************************
 *
 ***************************************************************************/
#ifdef PEPE
PUBLIC int msg2db_add_instance(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_msg_,  // owned
    cols_flag_t cols_flag,
    md_record_t *md_record
)
{
    md_record_t md_record_;
    json_t *jn_msg = 0;
    if(!md_record) {
        md_record = &md_record_;
    }

    if(cols_flag & fc_only_desc_cols) {
        // Esto por cada inserción? you are fool!
        json_t *topic = tranger_topic(tranger, topic_name);
        json_t *cols = kw_get_dict(topic, "cols", 0, 0);
        JSON_INCREF(cols);
        jn_msg = kw_clone_by_keys(jn_msg_, cols, FALSE); // TODO los test fallan si pongo true
    } else {
        jn_msg = jn_msg_;
    }

    if(tranger_append_record(
        tranger,
        topic_name,
        0, // __t__,         // if 0 then the time will be set by TimeRanger with now time
        0, // user_flag,
        md_record,
        jn_msg // owned
    )<0) {
        // Error already logged
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Load messages in a dict() of keys,
 *  and each key with a instances of value
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
)
{
    if(!jn_record) {
        jn_record = tranger_read_record_content(tranger, topic, md_record);
    }
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
    if(jn_record) {
        /*
         *  Match fields
         */
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

        /*
         *  Select fields
         */
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
    json_t *instance = jn_record?jn_record:json_object();
    json_t *jn_record_md = tranger_md2json(md_record);
    json_object_set_new(instance, "__md_tranger__", jn_record_md);

    /*
     *  Check active
     *  The last loaded msg will be the active msg
     */
    BOOL is_active = TRUE;

    /*
     *  Filter by callback
     */
    msg2db_instance_callback_t msg2db_instance_callback =
        (msg2db_instance_callback_t)(size_t)kw_get_int(
        list,
        "msg2db_instance_callback",
        0,
        0
    );
    if(msg2db_instance_callback) {
        int ret = msg2db_instance_callback(
            tranger,
            list,
            is_active,
            instance
        );

        if(ret < 0) {
            JSON_DECREF(instance);
            return -1;  // break the load
        } else if(ret>0) {
            // continue below, add the instance
        } else { // == 0
            JSON_DECREF(instance);
            return 0;  // Timeranger does not load the record, it's me.
        }
    }

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
 *
 ***************************************************************************/
PUBLIC json_t *msg2db_open_list(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_filter  // owned
)
{
    json_t *jn_list = json_pack("{s:s, s:o, s:I, s:o}",
        "topic_name", topic_name,
        "match_cond", jn_filter?jn_filter:json_object(),
        "load_record_callback", (json_int_t)(size_t)load_record_callback,
        "messages", json_object()
    );

    json_t *list = tranger_open_list(
        tranger,
        jn_list // owned
    );
    return list;
}

#endif

