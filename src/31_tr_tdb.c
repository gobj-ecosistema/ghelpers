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
PRIVATE char *build_trtdb_data_path(
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
PRIVATE char *build_trtdb_indexes_path(
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
PUBLIC json_t *trtdb_open_db( // Return IS NOT YOURS!
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
        topic_cols_desc = _trtdb_create_topic_cols_desc();
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

    parse_schema_cols(
        topic_cols_desc,
        kw_get_list(tags_topic, "cols", 0, KW_REQUIRED)
    );

    /*------------------------------*
     *  Open/Create "user" topics
     *------------------------------*/
    int idx;
    json_t *schema_topic;
    json_array_foreach(jn_schema_topics, idx, schema_topic) {
        const char *topic_name = kw_get_str(schema_topic, "topic_name", "", 0);
        if(empty_string(topic_name)) {
            continue;
        }
        json_t *topic = tranger_create_topic(
            tranger,    // If topic exists then only needs (tranger,name) parameters
            topic_name,
            kw_get_str(schema_topic, "pkey", "", 0),
            kw_get_str(schema_topic, "tkey", "", 0),
            tranger_str2system_flag(kw_get_str(schema_topic, "system_flag", "", 0)),
            json_incref(kw_get_list(schema_topic, "cols", 0, 0))
        );

        parse_schema_cols(
            topic_cols_desc,
            kw_get_list(topic, "cols", 0, KW_REQUIRED)
        );
    }

    /*------------------------------*
     *  Create the root of treedb
     *------------------------------*/
    json_t *treedbs = kw_get_dict(tranger, "treedbs", json_object(), KW_CREATE);
    trtdb = kw_get_dict(treedbs, treedb_name, json_object(), KW_CREATE);

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

    char path[NAME_MAX];
    build_trtdb_data_path(path, sizeof(path), treedb_name, tags_topic_name);
    kw_get_str(list, "trtdb_data_path", path, KW_CREATE);
    build_trtdb_indexes_path(path, sizeof(path), treedb_name, tags_topic_name);
    kw_get_str(list, "trtdb_indexes_path", path, KW_CREATE);

    kw_get_subdict_value(trtdb, tags_topic_name, "data", json_array(), KW_CREATE);
    kw_get_subdict_value(trtdb, tags_topic_name, "indexes", json_object(), KW_CREATE);

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

        build_trtdb_data_path(path, sizeof(path), treedb_name, topic_name);
        kw_get_str(list, "trtdb_data_path", path, KW_CREATE);
        build_trtdb_indexes_path(path, sizeof(path), treedb_name, topic_name);
        kw_get_str(list, "trtdb_indexes_path", path, KW_CREATE);

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
PUBLIC json_t *_trtdb_create_topic_cols_desc(void)
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
        json_pack("{s:s, s:s, s:s, s:[s,s,s,s,s,s], s:s}",
            "id", "flag",
            "header", "Flag",
            "type", "enum",
            "enum",
                "","persistent","required","volatil","uuid","include",
            "flag",
                ""
        )
    );
    return topic_cols_desc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_field(json_t *desc, json_t *data)
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
     *  Check field required
     */
    if(kw_has_word(desc_flag, "required", 0)) {
        if(!value) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "Wrong enum type",
                                    "desc",         "%j", desc,
                                    "data",         "%j", data,
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
                                "desc",         "%j", desc,
                                "data",         "%j", data,
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
                        "desc",         "%j", desc,
                        "data",         "%j", data,
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
                    "desc",         "%j", desc,
                    "data",         "%j", data,
                    "field",        "%s", desc_id,
                    "value",        "%s", json_string_value(value),
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Wrong basic type",
                    "desc",         "%j", desc,
                    "data",         "%j", data,
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
            "cols_desc",    "%j", cols_desc,
            NULL
        );
        return -1;
    }

    int idx; json_t *desc;

    if(json_is_object(data)) {
        json_array_foreach(cols_desc, idx, desc) {
            ret += check_field(desc, data);
        }
    } else if(json_is_array(data)) {
        int idx1; json_t *d;
        json_array_foreach(data, idx1, d) {
            int idx2;
            json_array_foreach(cols_desc, idx2, desc) {
                ret += check_field(desc, d);
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
PRIVATE json_t *get_id( // Return is not YOURS!
    json_t *topic,
    json_t *record // not owned
)
{
    const char *pkey = kw_get_str(topic, "pkey", "", KW_REQUIRED);
    return kw_get_dict_value(record, pkey, 0, 0);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trtdb_read_node(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id,     // owned, Can be: integer,string, [integer], [string], [keys]
    json_t *fields, // owned, Return only this fields. Can be: string, [string], [keys]
    json_t *kw,     // owned, Being filter on reading or record on writting
    const char *options // "create", TODO "delete",
)
{
    /*-------------------------------*
     *      Get data and indexes
     *-------------------------------*/
    char path[NAME_MAX];
    build_trtdb_data_path(path, sizeof(path), treedb_name, topic_name);
    json_t *data = kw_get_list(
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    build_trtdb_indexes_path(path, sizeof(path), treedb_name, topic_name);
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "TreeDb Topic data NOT FOUND",
            "path",         "%s", path,
            NULL
        );
        JSON_DECREF(id);
        JSON_DECREF(fields);
        JSON_DECREF(kw);
        return 0;
    }
    if(!indexes) {
        log_error(
            LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "TreeDb Topic indexes NOT FOUND",
            "path",         "%s", path,
            NULL
        );
        JSON_DECREF(id);
        JSON_DECREF(fields);
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
            // TODO filtra por fields
            JSON_DECREF(fields);
            JSON_INCREF(record);
            return record;
        }
        /*
         *  Not found, create if option
         */
        if(!(options && strstr(options, "create"))) {
            // No create
            JSON_DECREF(id);
            JSON_DECREF(fields);
            JSON_DECREF(kw);
            return 0;
        }
        JSON_INCREF(id);
        JSON_INCREF(kw);
        int ret = trtdb_write_node(
            tranger,
            treedb_name,
            topic_name,
            id, // owned
            kw, // owned
            options
        );
        if(ret < 0) {
            JSON_DECREF(id);
            JSON_DECREF(fields);
            JSON_DECREF(kw);
            return 0;
        }
        return trtdb_read_node(
            tranger,
            treedb_name,
            topic_name,
            id,     // owned
            fields, // owned
            kw,     // owned
            options
        );
    } else {
        /*-------------------------------*
         *      Working without id
         *  NOTE Must return a list
         *-------------------------------*/
        JSON_INCREF(fields);
        JSON_INCREF(kw);
        json_t *record = _trtdb_select(
            data,   // not owned
            fields, // owned, fields
            kw,     // owned, filter HACK use kw as filter
            0       // match fn
        );
        if(json_array_size(record)>0) {
            /*
             *  Found
             */
            JSON_DECREF(kw);
            // TODO filtra por fields
            JSON_DECREF(fields);
            return record;
        }
        JSON_DECREF(record);

        /*
         *  Not found, create if option
         */
        if(!(options && strstr(options, "create"))) {
            JSON_DECREF(fields);
            JSON_DECREF(kw);
            return 0;
        }

        /*
         *  Not found, create if option
         */
        JSON_INCREF(kw);
        int ret = trtdb_write_node(
            tranger,
            treedb_name,
            topic_name,
            0,
            kw, // owned
            options
        );
        if(ret < 0) {
            JSON_DECREF(fields);
            JSON_DECREF(kw);
            return json_array();
        }
        return trtdb_read_node(
            tranger,
            treedb_name,
            topic_name,
            0,      // owned
            fields, // owned
            kw,     // owned
            options
        );
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_field_value(
    json_t *topic,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Col desc without 'id'",
            "topic_name",   "%s", kw_get_str(topic, "topic_name", "", KW_REQUIRED),
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Col desc without 'type'",
            "topic_name",   "%s", kw_get_str(topic, "topic_name", "", KW_REQUIRED),
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Field required",
                "topic_name",   "%s", kw_get_str(topic, "topic_name", "", KW_REQUIRED),
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Field cannot be null",
                "topic_name",   "%s", kw_get_str(topic, "topic_name", "", KW_REQUIRED),
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

    if(kw_has_word(desc_flag, "persistent", 0) || kw_has_word(desc_flag, "volatil", 0)) {
    }

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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Value must be string",
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Value must be integer",
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Value must be real",
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
            if(JSON_TYPEOF(value)==JSON_ARRAY) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_array());
            }
            break;

        CASES("object")
            if(JSON_TYPEOF(value)==JSON_OBJECT) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_object());
            }
            break;

        DEFAULTS
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Col type unknown",
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
PRIVATE json_t *create_record(
    json_t *topic,  // not owned
    json_t *id, // not owned
    json_t *kw,  // not owned
    const char *options
)
{
    const char *pkey = kw_get_str(topic, "pkey", "", KW_REQUIRED);
    json_t *cols = kw_get_list(topic, "cols", 0, KW_REQUIRED);

    json_t *new_record = json_object();

    int idx; json_t *col;
    json_array_foreach(cols, idx, col) {
        const char *field = kw_get_str(col, "id", 0, 0);
        if(strcmp(field, pkey)==0) {
            if(id) {
                /*
                 *  Explicit id
                 */
                if(set_field_value(topic, col, new_record, id)<0) {
                    // Error already logged
                    JSON_DECREF(new_record);
                    return 0;
                }
                continue;
            }
        }

        if(set_field_value(topic, col, new_record, kw_get_dict_value(kw, field, 0, 0))<0) {
            // Error already logged
            JSON_DECREF(new_record);
            return 0;
        }
    }

    // TODO check cols id with uuid

    if(options && !strstr(options, "strict")) {
        json_object_update_missing(new_record, kw);
        json_object_del(new_record, "__md_treedb__");
    }

    return new_record;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int trtdb_write_node(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id, // owned
    json_t *kw, // owned
    const char *options // "strict"
)
{
    json_t *topic = tranger_topic(tranger, topic_name);

    json_t *new_record = create_record(topic, id, kw, options);
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
PRIVATE json_t *_md2json(json_t *topic, md_record_t *md_record)
{
    json_t *jn_md = json_object();
    json_object_set(
        jn_md,
        "topic_name",
        json_object_get(topic, "topic_name")
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
        kw_get_str(list, "trtdb_data_path", 0, KW_REQUIRED),
        0,
        KW_REQUIRED
    );
    json_t *indexes = kw_get_dict(
        tranger,
        kw_get_str(list, "trtdb_indexes_path", 0, KW_REQUIRED),
        0,
        KW_REQUIRED
    );
    if(!data || !indexes) {
        JSON_DECREF(jn_record);
        return -1;  // Timeranger: break the load
    }

    /*
     *  Exists already the id?
     */
    json_t *record = kw_get_dict(indexes, key, 0, 0);
    if(!record) {
        /*
         *  New record
         */
        json_t *jn_record_md = _md2json(topic, md_record);
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

    JSON_DECREF(jn_record);
    return 0;  // Timeranger: does not load the record, it's me.
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
    if(json_is_array(kw)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw, idx, jn_value) {
            KW_INCREF(jn_filter);
            if(match_fn(jn_value, jn_filter)) {
                json_array_append(kw_new, jn_value);
            }
        }
    } else if(json_is_object(kw)) {
        KW_INCREF(jn_filter);
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

    JSON_DECREF(jn_fields);
    JSON_DECREF(jn_filter);
    return kw_new;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trtdb_link_node(
    json_t *tranger,
    const char *treedb_name,
    const char *slink,
    json_t *kw_parent,
    json_t *kw_child,
    const char *options  // TODO "return-child" (default), "return-parent"
)
{
    // TODO mira si en un explicito de un link multiple split(slink, ".")

    json_t *link = kw_get_dict_value(kw_parent, slink, 0, 0);
    if(!link) {
        log_error(
            LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Property 'link' not found",
            "data",         "%j", kw_parent,
            NULL
        );
        return 0;
    }
    switch(json_typeof(link)) { // json_typeof CONTROLADO
    case JSON_ARRAY:
        // TODO el split tiene que venir
        log_error(
            LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "'link' bad type",
            "link",         "%j", link,
            NULL
        );
        break;
    case JSON_STRING:
        {
            kw_set_dict_value(
                kw_parent,
                json_string_value(link),
                get_id(
                    tranger_topic(
                        tranger,
                        kw_get_str(kw_child, "__md_treedb__`topic_name", 0, KW_REQUIRED)
                    ),
                    kw_child
                )
            );
        }
        break;
    default:
        log_error(
            LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "'link' bad type",
            "link",         "%j", link,
            NULL
        );
        break;
    }


    json_t *child = 0;
    // TODO

    return child;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trtdb_unlink_node(
    json_t *tranger,
    const char *treedb_name,
    const char *link,
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
    // TODO
    return 0;
}

PUBLIC int trtdb_end_transaction(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *options
)
{
    // TODO
    return 0;
}

