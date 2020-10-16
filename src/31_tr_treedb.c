/***********************************************************************
 *          TR_TREEDB.C
 *
 *          Tree Database with TimeRanger
 *          Hierarchical tree of objects (nodes, records, messages)
 *          linked by child-parent relation.
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

PRIVATE int load_all_links(
    json_t *tranger,
    const char *treedb_name
);
PRIVATE int load_id_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
);
PRIVATE int load_pkey2_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
);

PRIVATE json_t *get_hook_refs(
    json_t *hook_data, // not owned
    BOOL original_node
);
PRIVATE json_t *get_fkey_refs(
    json_t *field_data // not owned
);
PRIVATE int _link_nodes(
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
);
PRIVATE int _unlink_nodes(
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
);

PRIVATE json_t * treedb_get_activated_snap_tag(
    json_t *tranger,
    const char *treedb_name,
    uint32_t *user_flag
);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE json_t *topic_cols_desc = 0;
PRIVATE BOOL treedb_trace = 0;

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int current_snap_tag(json_t *tranger, const char *treedb_name)
{
    char path[NAME_MAX];
    snprintf(path, sizeof(path), "treedbs_snaps`%s`activated_snap_tag", treedb_name);

    return kw_get_int(tranger, path, 0, KW_REQUIRED);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_topic_pkey2s( // Return MUST be decref, a dict with pkey2s
    json_t *tranger,
    const char *topic_name
)
{
    json_t *topic_desc = kw_get_subdict_value(tranger, "topics", topic_name, 0, 0);
    if(!topic_desc) {
        // Silence
        return json_object();
    }
    json_t *dict = kw_get_dict(topic_desc, "topic_pkey2s", 0, 0);
    if(!dict) {
        // Silence
        return json_object();
    }
    return json_incref(dict);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *build_id_index_path(
    char *bf,
    int bfsize,
    const char *treedb_name,
    const char *topic_name
)
{
    snprintf(bf, bfsize, "treedbs`%s`%s`id", treedb_name, topic_name);
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *build_pkey_index_path(
    char *bf,
    int bfsize,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name
)
{
    snprintf(bf, bfsize, "treedbs`%s`%s`%s", treedb_name, topic_name, pkey2_name);
    return bf;
}

/***************************************************************************
 * PUBLIC to use in tests
 ***************************************************************************/
PUBLIC json_t *treedb_get_id_index( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    char path[NAME_MAX];
    build_id_index_path(path, sizeof(path), treedb_name, topic_name);
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
PRIVATE json_t *treedb_get_pkey2_index( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name
)
{
    // TODO de momento sin claves combinadas, el nombre de la pkey es el nombre del field
    char path[NAME_MAX];
    build_pkey_index_path(path, sizeof(path), treedb_name, topic_name, pkey2_name);
    json_t *indexy = kw_get_dict(
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    return indexy;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *get_key2_value(
    json_t *tranger,
    const char *topic_name,
    const char *pkey2_name,
    json_t *kw
)
{
    // TODO combined no, de momento solo claves simples
    const char *pkey2_value = kw_get_str(kw, pkey2_name, "", 0);
    return pkey2_value;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *exist_primary_node(
    json_t *indexx,
    const char *key
)
{
    return json_object_get(indexx, key);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_primary_node(
    json_t *indexx,
    const char *key,
    json_t *node // incref
)
{
    return json_object_set(indexx, key, node);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int delete_primary_node(
    json_t *indexx,
    const char *key
)
{
    return json_object_del(indexx, key);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *_treedb_create_topic_cols_desc(void)
{
    json_t *topic_cols_desc = json_array();
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s]}",
            "id", "id",
            "header", "Id",
            "fillspace", 10,
            "type",
                "string",
            "flag",
                "required"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i,s:s, s:[]}",
            "id", "header",
            "header", "Header",
            "fillspace", 10,
            "type",
                "string",
            "flag"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[]}",
            "id", "fillspace",
            "header", "Fillspace",
            "fillspace", 4,
            "type",
                "integer",
            "flag"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s,s,s,s,s,s,s,s,s], s:[s,s]}",
            "id", "type",
            "header", "Type",
            "fillspace", 10,
            "type", "enum",
            "enum",
                "string","integer","object","dict","array","list","real","boolean","enum","blob",
            "flag",
                "required", "notnull"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s], s:[]}",
            "id", "flag",
            "header", "Flag",
            "fillspace", 10,
            "type", "enum",
            "enum",
                "","persistent","required","fkey",
                "hook","uuid","notnull","wild","rowid","inherit",
                "readable","writable","stats","rstats","pstats",
                "password","email","url",
            "flag"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[]}",
            "id", "default",
            "header", "Default",
            "fillspace", 10,
            "type",
                "blob",
            "flag"
        )
    );

    return topic_cols_desc;
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
            then you must change the schema version and topic_version
 ***************************************************************************/
PUBLIC json_t *treedb_open_db( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
)
{
    BOOL master = kw_get_bool(tranger, "master", 0, KW_REQUIRED);

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

    char schema_full_path[NAME_MAX*2];
    snprintf(schema_full_path, sizeof(schema_full_path), "%s/%s",
        kw_get_str(tranger, "directory", "", KW_REQUIRED),
        schema_filename
    );

    if(options && strstr(options,"persistent")) {
        json_int_t schema_new_version = kw_get_int(jn_schema, "schema_version", 0, KW_WILD_NUMBER);
        do {
            BOOL recreating = FALSE;
            if(file_exists(schema_full_path, 0)) {
                json_t *old_jn_schema = load_json_from_file(
                    schema_full_path,
                    "",
                    kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED)
                );
                if(!master) {
                    JSON_DECREF(jn_schema);
                    jn_schema = old_jn_schema;
                    break; // Nothing to do
                }
                json_int_t schema_old_version = kw_get_int(
                    old_jn_schema,
                    "schema_version",
                    -1,
                    KW_WILD_NUMBER
                );
                if(schema_new_version <= schema_old_version) {
                    JSON_DECREF(jn_schema);
                    jn_schema = old_jn_schema;
                    break; // Nothing to do
                } else {
                    recreating = TRUE;
                    JSON_DECREF(old_jn_schema);
                }
            }
            log_info(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", recreating?"Re-Creating TreeDB schema file":"Creating TreeDB schema file",
                "treedb_name",  "%s", treedb_name,
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

        } while(0);

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

    /*------------------------------*
     *  Create the root of treedb
     *------------------------------*/
    json_t *treedbs = kw_get_dict(tranger, "treedbs", json_object(), KW_CREATE);
    treedb = kw_get_dict(treedbs, treedb_name, json_object(), KW_CREATE);

    /*-------------------------------*
     *  Create "system" topics:
     *      __snaps__ topic
     *-------------------------------*/
    char *snaps_topic_name = "__snaps__";
    const char *snaps_topic_version = "1";
    json_t *jn_snaps_topic_var = json_object();
    json_object_set_new(jn_snaps_topic_var, "topic_version", json_string(snaps_topic_version));

    json_t *tag_schema = json_pack(
        "{s:{s:s, s:s, s:i, s:s, s:[s,s,s]},"
            "s:{s:s, s:s, s:i, s:s, s:[s,s]},"
            "s:{s:s, s:s, s:i, s:s, s:[s,s]},"
            "s:{s:s, s:s, s:i, s:s, s:[s,s]}}",
        "id",
            "id", "id",
            "header", "Id",
            "fillspace", 8,
            "type", "string",
            "flag",
                "persistent","required","rowid",

        "name",
            "id", "name",
            "header", "Name",
            "fillspace", 30,
            "type", "string",
            "flag",
                "persistent", "required",
        "date",
            "id", "date",
            "header", "Date",
            "fillspace", 30,
            "type", "string",
            "flag",
                "persistent", "required",
        "active",
            "id", "active",
            "header", "Active",
            "fillspace", 8,
            "type", "boolean",
            "flag",
                "persistent", "required"
    );
    if(!tag_schema) {
        log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Bad __snaps__ json schema",
            NULL
        );
    }

    json_t *snaps_topic = tranger_create_topic( // System topic
        tranger,    // If topic exists then only needs (tranger,name) parameters
        snaps_topic_name,
        "id",
        "",
        sf_string_key,
        tag_schema,
        jn_snaps_topic_var
    );

    parse_schema_cols(
        topic_cols_desc,
        kwid_new_list("verbose", snaps_topic, "cols")
    );

    /*------------------------------*
     *  Open "system" lists:
     *      __snaps__ topic
     *------------------------------*/
    char path[NAME_MAX];
    build_id_index_path(path, sizeof(path), treedb_name, snaps_topic_name);
    kw_get_dict_value(tranger, path, json_object(), KW_CREATE);

    json_t *jn_filter = json_pack("{s:b}",
        "backward", 1
    );

    json_t *jn_list = json_pack("{s:s, s:s, s:o, s:I, s:s, s:{}}",
        "id", path,
        "topic_name", snaps_topic_name,
        "match_cond", jn_filter,
        "load_record_callback", (json_int_t)(size_t)load_id_callback,
        "treedb_name", treedb_name,
        "deleted_records"
    );
    tranger_open_list(
        tranger,
        jn_list // owned
    );

    /*------------------------------*
     *  Get snap tab
     *------------------------------*/
    json_t *treedb_snaps = kw_get_dict(tranger, "treedbs_snaps", json_object(), KW_CREATE);

    uint32_t snap_tag = 0;
    treedb_get_activated_snap_tag(
        tranger,
        treedb_name,
        &snap_tag
    );
    if(snap_tag) {
        char temp[80];
        snprintf(temp, sizeof(temp), "loading snap_tag %d", snap_tag);
        log_info(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", temp,
            "snap_tag",     "%d", (int)snap_tag,
            NULL
        );
    }

    // Save current snap tag
    json_t *treedb_snap = kw_get_dict(treedb_snaps, treedb_name, json_object(), KW_CREATE);
    json_object_set_new(treedb_snap, "activated_snap_tag", json_integer(snap_tag));

    /*------------------------------*
     *  Create "user" topics
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
        const char *system_flag = kw_get_str(schema_topic, "system_flag", "", 0);
        if(strcmp(system_flag, "sf_string_key")!=0) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Schema topic without system_flag=sf_string_key",
                "treedb_name",  "%s", treedb_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        const char *topic_version = kw_get_str(schema_topic, "topic_version", "", 0);
        const char *topic_tkey = kw_get_str(schema_topic, "tkey", "", 0);
        json_t *topic_pkey2s = kw_get_dict_value(schema_topic, "topic_pkey2s", 0, 0);

        treedb_create_topic(
            tranger,
            treedb_name,
            topic_name,
            topic_version,
            topic_tkey,
            json_incref(topic_pkey2s),
            kwid_new_dict("verbose", schema_topic, "cols"), // owned
            snap_tag,
            FALSE // create_schema
        );
    }

    /*------------------------------*
     *  Parse hooks
     *------------------------------*/
    parse_hooks(tranger);

    /*------------------------------*
     *  Load links
     *------------------------------*/
    load_all_links(tranger, treedb_name);

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
     *  Close treedb topics
     *------------------------------*/
    json_t *topics = treedb_topics(tranger, treedb_name, 0);
    int idx; json_t *topic;
    json_array_foreach(topics, idx, topic) {
        const char *topic_name = json_string_value(topic);
        treedb_close_topic(tranger, treedb_name, topic_name);
    }
    JSON_DECREF(topics);

    /*------------------------------*
     *  Delete treedb
     *------------------------------*/
    json_t *treedb = kw_get_subdict_value(tranger, "treedbs", treedb_name, 0, KW_EXTRACT);
    JSON_DECREF(treedb);

    JSON_DECREF(topic_cols_desc);
    return 0;
}

/***************************************************************************
    Return is NOT YOURS, pkey MUST be "id"
    WARNING This function don't load hook links.
    HACK IDEMPOTENT function

    pkey2s: 1) string -> only one simple key (name of pkey2 is the name of topic field
      TODO  2) dict   -> several types of keys -> { name of pkey2: pkey2 type and his fields}
                values of dict:
                        string: one simple key, same as 1)
                        list of strings: combined key
 ***************************************************************************/
PUBLIC json_t *treedb_create_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *topic_version,
    const char *topic_tkey,
    json_t *topic_pkey2s, // owned, string or dict of string | [strings]
    json_t *cols, // owned
    uint32_t snap_tag,
    BOOL create_schema
)
{
    if(empty_string(treedb_name)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "treedb_name empty",
            NULL
        );
        JSON_DECREF(topic_pkey2s);
        JSON_DECREF(cols);
        return 0;
    }
    if(empty_string(topic_name)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "topic_name empty",
            NULL
        );
        JSON_DECREF(topic_pkey2s);
        JSON_DECREF(cols);
        return 0;
    }

    /*------------------------------*
     *  Get treedb list
     *------------------------------*/
    json_t *treedb = kw_get_subdict_value(tranger, "treedbs", treedb_name, 0, 0);
    if(!treedb) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB not found.",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(topic_pkey2s);
        JSON_DECREF(cols);
        return 0;
    }

    /*------------------------------*
     *  Check if already exists
     *------------------------------*/
    json_t *topic = kw_get_dict(treedb, topic_name, 0, 0);
    if(topic) {
        JSON_DECREF(topic_pkey2s);
        JSON_DECREF(cols);
        return topic;
    }

    /*------------------------------*
     *  Open/Create "user" topic
     *------------------------------*/
    // Topic version
    json_t *jn_topic_var = json_object();
    json_object_set_new(jn_topic_var, "topic_version", json_string(topic_version?topic_version:""));

    // Topic pkey2s
    if(topic_pkey2s) {
        if(json_is_string(topic_pkey2s)) {
            json_t *dict = json_object();
            json_object_set(dict, json_string_value(topic_pkey2s), topic_pkey2s);
            json_object_set_new(jn_topic_var, "topic_pkey2s", dict);
        } else if(json_is_object(topic_pkey2s)) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "combined pkey2s NOT IMPLEMENTED",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                NULL
            );
        } else {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "pkey2s INVALID TYPE",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                NULL
            );
            log_debug_json(0, topic_pkey2s, "pkey2s INVALID TYPE");
        }
        JSON_DECREF(topic_pkey2s);
    }

    JSON_INCREF(cols);
    JSON_INCREF(jn_topic_var);
    topic = tranger_create_topic( // User topic
        tranger,    // If topic exists then only needs (tranger,name) parameters
        topic_name,
        "id",       // HACK pkey fixed
        topic_tkey, // tkey
        tranger_str2system_flag("sf_string_key"), // HACK system_flag fixed
        cols,           // owned below
        jn_topic_var    // owned below
    );

    parse_schema_cols(
        topic_cols_desc,
        kwid_new_list("verbose", topic, "cols")
    );

    if(create_schema) {
        json_t *jn_schema = json_object();
        kw_get_str(jn_schema, "topic_name", topic_name, KW_CREATE);
        kw_get_str(jn_schema, "pkey", "id", KW_CREATE);
        kw_get_str(jn_schema, "system_flag", "sf_string_key", KW_CREATE);
        kw_get_dict(jn_schema, "cols", cols, KW_CREATE); // cols owned
        json_object_update(jn_schema, jn_topic_var);

        char schema_filename[NAME_MAX];
        snprintf(schema_filename, sizeof(schema_filename), "%s-%s.treedb_schema.json",
            treedb_name,
            topic_name
        );

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
    } else {
        JSON_DECREF(cols);
    }

    /*------------------------------------*
     *      Open "user" lists
     *------------------------------------*/
    char path[NAME_MAX];

    /*----------------------*
     *   Main index: "id"
     *----------------------*/
    build_id_index_path(path, sizeof(path), treedb_name, topic_name);
    kw_get_dict_value(tranger, path, json_object(), KW_CREATE);

    json_t *jn_filter = json_pack("{s:b}",
        "backward", 1
    );
    if(snap_tag) {
        // pon el current tag
        json_object_set_new(
            jn_filter,
            "user_flag",
            json_integer(snap_tag)
        );
    }
    json_t *jn_list = json_pack("{s:s, s:s, s:i, s:o, s:I, s:s, s:{}}",
        "id", path,
        "topic_name", topic_name,
        "snap_tag", (int)snap_tag,
        "match_cond", jn_filter,
        "load_record_callback", (json_int_t)(size_t)load_id_callback,
        "treedb_name", treedb_name,
        "deleted_records"
    );
    tranger_open_list(
        tranger,
        jn_list // owned
    );

    /*----------------------*
     *   Secondary indexes
     *----------------------*/
    json_t *iter_pkey2 = kw_get_dict(jn_topic_var, "topic_pkey2s", 0, 0);
    const char *pkey2_name; json_t *jn_pkey2_fields;
    json_object_foreach(iter_pkey2, pkey2_name, jn_pkey2_fields) {
        const char *pkey2_fields = json_string_value(jn_pkey2_fields);
        if(!pkey2_fields) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "combined pkey2s NOT IMPLEMENTED",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                NULL
            );
            continue;
        }

        build_pkey_index_path(path, sizeof(path), treedb_name, topic_name, pkey2_name);
        kw_get_dict_value(tranger, path, json_object(), KW_CREATE);

        json_t *jn_filter = json_pack("{s:b}",
            "backward", 1
        );
        json_t *jn_list = json_pack("{s:s, s:s, s:i, s:o, s:I, s:s, s:s, s:{}}",
            "id", path,
            "topic_name", topic_name,
            "snap_tag", (int)snap_tag,
            "match_cond", jn_filter,
            "load_record_callback", (json_int_t)(size_t)load_pkey2_callback,
            "treedb_name", treedb_name,
            "pkey2_name", pkey2_name,
            "deleted_records"
        );
        tranger_open_list(
            tranger,
            jn_list // owned
        );
    }
    JSON_DECREF(jn_topic_var);

    return topic;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_close_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    /*------------------------------*
     *  Close id list
     *------------------------------*/
    json_t *treedb = kw_get_subdict_value(tranger, "treedbs", treedb_name, 0, 0);
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

    char path[NAME_MAX];
    build_id_index_path(path, sizeof(path), treedb_name, topic_name);
    json_t *list = tranger_get_list(tranger, path);
    if(list) {
        tranger_close_list(tranger, list);
    } else {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "List not found.",
            "treedb_name",  "%s", treedb_name,
            "list",         "%s", path,
            NULL
        );
    }

    /*------------------------------*
     *  Close pkey2 lists
     *------------------------------*/
    json_t *iter_pkey2s = treedb_topic_pkey2s(tranger, topic_name);
    const char *pkey2_name; json_t *jn_pkey2_fields;
    json_object_foreach(iter_pkey2s, pkey2_name, jn_pkey2_fields) {
        const char *pkey2_fields = json_string_value(jn_pkey2_fields);
        if(!pkey2_fields) {
            continue;
        }
        build_pkey_index_path(path, sizeof(path), treedb_name, topic_name, pkey2_name);

        json_t *list = tranger_get_list(tranger, path);
        if(list) {
            tranger_close_list(tranger, list);
        } else {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "List not found.",
                "treedb_name",  "%s", treedb_name,
                "list",         "%s", path,
                NULL
            );
        }
    }
    JSON_DECREF(iter_pkey2s);

    /*----------------------*
     *  Remove topic data
     *----------------------*/
    snprintf(path, sizeof(path), "treedbs`%s`%s", treedb_name, topic_name);
    json_t *data = kw_get_dict(
        tranger,
        path,
        0,
        KW_REQUIRED|KW_EXTRACT
    );
    json_decref(data);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_delete_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    treedb_close_topic(tranger, treedb_name, topic_name);
    return tranger_delete_topic(tranger, topic_name);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_list_treedb(
    json_t *tranger
)
{
    json_t *treedb_list = json_array();

    json_t *treedbs = kw_get_dict(tranger, "treedbs", 0, 0);
    if(!treedbs) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "NO TreeDB found.",
            NULL
        );
        return treedb_list;
    }
    const char *treedb_name; json_t *treedb;
    json_object_foreach(treedbs, treedb_name, treedb) {
        json_array_append_new(treedb_list, json_string(treedb_name));
    }

    return treedb_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_topics(
    json_t *tranger,
    const char *treedb_name,
    const char *options // "dict" return list of dicts, otherwise return list of strings
)
{
    json_t *treedb = kw_get_subdict_value(tranger, "treedbs", treedb_name, 0, 0);
    if(!treedb) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB not found.",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        return 0;
    }

    json_t *topic_list = json_array();

    char list_id[NAME_MAX];
    const char *topic_name; json_t *topic_records;
    json_object_foreach(treedb, topic_name, topic_records) {
        build_id_index_path(list_id, sizeof(list_id), treedb_name, topic_name);
        if(options && strstr(options, "dict")) {
            json_t *dict = json_object();
            json_object_set_new(dict, "id", json_string(list_id));
            json_object_set_new(dict, "value", json_string(topic_name));
            json_array_append_new(topic_list, dict);
        } else {
            json_array_append_new(topic_list, json_string(topic_name));
        }
    }

    return topic_list;
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
        } else if(strcasecmp(json_string_value(field), "blob")==0) {
            return "blob";
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
     *  Get value
     */
    json_t *value = kw_get_dict_value(dato, desc_id, 0, 0);

    /*
     *  Check required
     */
    if(kw_has_word(desc_flag, "required", 0)) {
        if(!value) {
            log_error(LOG_OPT_TRACE_STACK,
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
         *      Enum
         *----------------------------*/
        CASES("blob")
            break;

        /*----------------------------*
         *      Json basic types
         *----------------------------*/
        DEFAULTS
            if(value) {
                const char *value_type = my_json_type(value);
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

/***************************************************************************
 * Return a list of hook field names of the topic.
 * Return MUST be decref
 ***************************************************************************/
PUBLIC json_t *topic_desc_hook_names(
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
 * Return a list of fkeys field names of the topic.
 * Return MUST be decref
 ***************************************************************************/
PUBLIC json_t *topic_desc_fkey_names(
    json_t *topic_desc // owned
)
{
    json_t *jn_fkey_field_names = json_array();

    int idx; json_t *col;
    json_array_foreach(topic_desc, idx, col) {
        json_t *flag = kw_get_dict_value(col, "flag", 0, 0);
        /*-------------------------*
         *      Is a fkey?
         *-------------------------*/
        if(kw_has_word(flag, "fkey", 0)) {
            json_array_append(jn_fkey_field_names, json_object_get(col, "id"));
        }
    }
    JSON_DECREF(topic_desc);
    return jn_fkey_field_names;
}




                    /*------------------------------------*
                     *      Write/Read to/from tranger
                     *------------------------------------*/




/***************************************************************************
 *  Usado en set_tranger_field_value(), to write the fkey in tranger file
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
                    // String format
                    const char *id = json_string_value(v);
                    if(count_char(id, '^')==2) {
                        if(!mix_ids) {
                            mix_ids = json_array();
                        }
                        json_array_append_new(mix_ids, json_string(id));
                    }
                } else if(json_typeof(v)==JSON_OBJECT) {
                    // Message format **FKEY**
                    if(kw_has_key(v, "__md_fkey__")) {
                        if(!mix_ids) {
                            mix_ids = json_array();
                        }
                        json_array_append(mix_ids, v);
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
                } else if(json_typeof(v)==JSON_OBJECT) {
                    // Message format **FKEY**
                    if(kw_has_key(v, "__md_fkey__")) {
                        if(!mix_ids) {
                            mix_ids = json_array();
                        }
                        json_object_set(mix_ids, id, v);
                    }
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
        if(!value) { // WARNING efecto colateral? 16-oct-2020 || json_is_null(value)) {
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

    BOOL is_persistent = kw_has_word(desc_flag, "persistent", 0)?TRUE:FALSE;
    BOOL wild_conversion = kw_has_word(desc_flag, "wild", 0)?TRUE:FALSE;
    BOOL is_hook = kw_has_word(desc_flag, "hook", 0)?TRUE:FALSE;
    BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
    if(!(is_persistent || is_hook || is_fkey)) {
        // Not save to tranger
        return 0;
    }

    SWITCHS(type) {
        CASES("list")
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

        CASES("dict")
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

        CASES("blob")
            json_object_set(record, field, value);
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
PRIVATE int set_volatil_field_value(
    const char *type,
    const char *field,
    json_t *record, // not owned
    json_t *value   // not owned
)
{
    SWITCHS(type) {
        CASES("list")
        CASES("array")
            if(JSON_TYPEOF(value, JSON_ARRAY)) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_array());
            }
            break;

        CASES("dict")
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

        CASES("blob")
            json_object_set(record, field, value);
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
        if(!value && create) {
            value = kw_get_dict_value(col, "default", 0, 0);
        }
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
    json_object_set_new(jn_md, "__tm__", json_integer(md_record->__tm__));
    json_object_set_new(jn_md, "__tag__", json_integer(md_record->__user_flag__));
    json_object_set_new(jn_md, "__original_node__", json_true());

    return jn_md;
}

/***************************************************************************
 *  When record is loaded from disk then create the node
 *  when is loaded from memory then notify to subscribers
 ***************************************************************************/
PRIVATE int load_id_callback(
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
                const char *treedb_name = kw_get_str(list, "treedb_name", 0, KW_REQUIRED);
                const char *topic_name = kw_get_str(list, "topic_name", 0, KW_REQUIRED);

                /*----------------------------------*
                 *  Get indexx: to load from disk
                 *----------------------------------*/
                json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
                if(!indexx) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "TreeDb Topic indexx NOT FOUND",
                        "topic_name",   "%s", topic_name,
                        NULL
                    );
                    JSON_DECREF(jn_record);
                    return 0;  // Timeranger: does not load the record, it's mine.
                }

                /*-------------------------------*
                 *  Exists already the node?
                 *-------------------------------*/
                if(exist_primary_node(indexx, md_record->key.s)) {
                    // Ignore
                    // The node with this key already exists
                    // HACK using backward, the first record is the last record
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
                    json_object_set_new(jn_record_md, "__pending_links__", json_true());
                    json_object_set_new(jn_record, "__md_treedb__", jn_record_md);

                    /*--------------------------------------------*
                     *  Set volatil data
                     *--------------------------------------------*/
                    set_volatil_values( // crea campos vacios
                        tranger,
                        topic_name,
                        jn_record,  // not owned
                        jn_record // not owned
                    );

                    /*-------------------------------*
                     *  Write node in memory: id
                     *-------------------------------*/
                    add_primary_node(
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
 *  When record is loaded from disk then create the node
 *  when is loaded from memory then notify to subscribers
 ***************************************************************************/
PRIVATE int load_pkey2_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
)
{
    const char *pkey2_name = kw_get_str(list, "pkey2_name", "", KW_REQUIRED);
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
                const char *treedb_name = kw_get_str(list, "treedb_name", 0, KW_REQUIRED);
                const char *topic_name = kw_get_str(list, "topic_name", 0, KW_REQUIRED);

                /*---------------------------------*
                 *  Get indexy: to load from disk
                 *---------------------------------*/
                json_t *indexy = treedb_get_pkey2_index(
                    tranger,
                    treedb_name,
                    topic_name,
                    pkey2_name
                );
                if(!indexy) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "TreeDb Topic indexy NOT FOUND",
                        "topic_name",   "%s", topic_name,
                        "pkey2_name",   "%s", pkey2_name,
                        NULL
                    );
                    JSON_DECREF(jn_record);
                    return 0;  // Timeranger: does not load the record, it's mine.
                }

                const char *pkey2_value = get_key2_value(
                    tranger,
                    topic_name,
                    pkey2_name,
                    jn_record
                );

                /*-------------------------------*
                 *  Exists already the node?
                 *-------------------------------*/
                if(kw_get_subdict_value(
                        indexy,
                        md_record->key.s,
                        pkey2_value,
                        0,
                        KW_EMPTY_VALID
                    )!=0) {
                    // Ignore
                    // The node with this key already exists
                    // HACK using backward, the first record is the last record
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
                    json_object_set_new(jn_record_md, "pkey2_name", json_string(pkey2_name));

                    /*--------------------------------------------*
                     *  Set volatil data
                     *--------------------------------------------*/
                    set_volatil_values( // crea campos vacios
                        tranger,
                        topic_name,
                        jn_record,  // not owned
                        jn_record // not owned
                    );

                    /*-------------------------------*
                     *  Write node in memory: pkey2
                     *-------------------------------*/
                    JSON_INCREF(jn_record);
                    kw_set_subdict_value(
                        indexy,
                        md_record->key.s,
                        pkey2_value,
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
 *  Decode fkey
 ***************************************************************************/
PRIVATE BOOL decode_string_fkey(
    const char *pref,
    char *topic_name, int topic_name_size,
    char *id, int id_size,
    char *hook_name, int hook_name_size
)
{
    *topic_name = *id = *hook_name = 0;

    if(!(pref && strchr(pref, '^'))) {
        return FALSE;
    }
    int list_size;
    const char **ss = split2(pref, "^", &list_size);
    if(list_size!=3) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Bad fkey ref",
            "pref",         "%s", pref,
            NULL
        );
        split_free2(ss);
        return FALSE;
    }

    snprintf(topic_name, topic_name_size, "%s", ss[0]);
    snprintf(id, id_size, "%s", ss[1]);
    snprintf(hook_name, hook_name_size, "%s", ss[2]);

    split_free2(ss);
    return TRUE;

}

/***************************************************************************
 *  Decode child ref
 ***************************************************************************/
PRIVATE BOOL decode_child_ref(
    const char *pref,
    char *topic_name, int topic_name_size,
    char *id, int id_size
)
{
    *topic_name = *id = 0;

    if(!(pref && strchr(pref, '^'))) {
        return FALSE;
    }
    int list_size;
    const char **ss = split2(pref, "^", &list_size);
    if(list_size!=2) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Bad child ref",
            "pref",         "%s", pref,
            NULL
        );
        split_free2(ss);
        return FALSE;
    }

    snprintf(topic_name, topic_name_size, "%s", ss[0]);
    snprintf(id, id_size, "%s", ss[1]);

    split_free2(ss);
    return TRUE;

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL is_fkey_registered(
    json_t *fkey_desc,
    const char *parent_topic_name_,
    const char *hook_name_
)
{
    const char *parent_topic_name; json_t *jn_parent_field_name;
    json_object_foreach(fkey_desc, parent_topic_name, jn_parent_field_name) {
        const char *hook_name = json_string_value(jn_parent_field_name);
        if(strcmp(parent_topic_name, parent_topic_name_)==0 && strcmp(hook_name, hook_name_)==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Loading hook links
 ***************************************************************************/
PRIVATE int link_child_to_parent(
    json_t *tranger,
    const char *treedb_name,
    const char *pref,
    json_t *child_node,
    const char *fkey_col_name,
    json_t *fkey_desc,
    BOOL is_child_hook
)
{
    char parent_topic_name[NAME_MAX];
    char parent_id[NAME_MAX];
    char hook_name[NAME_MAX];
    if(!decode_string_fkey(
        pref,
        parent_topic_name, sizeof(parent_topic_name),
        parent_id, sizeof(parent_id),
        hook_name, sizeof(hook_name)
    )) {
        /*
         *  It's not a fkey.
         *  It's not an error, it happens when it's a hook and fkey field.
         */
        return 0;
    }

    /*
     *  Check if it's a registered desc fkey
     */
    if(!is_fkey_registered(fkey_desc, parent_topic_name, hook_name)) {
        // It's not for us
        return 0;
    }

    /*
     *  Find the parent node
     */
    json_t *parent_node = treedb_get_node( // Return is NOT YOURS
        tranger,
        treedb_name,
        parent_topic_name,
        parent_id,
        0
    );
    if(!parent_node) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Node not found",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", parent_topic_name,
            "fkey_col_name","%s", fkey_col_name,
            "id",           "%s", parent_id,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  In parent hook data: save child content
     *  WARNING repeated in
     *      _link_nodes() and link_child_to_parent()
     *--------------------------------------------------*/
    json_t *parent_hook_data = kw_get_dict_value(parent_node, hook_name, 0, 0);
    if(!parent_hook_data) {
        log_error(0,
            "gobj",             "%s", __FILE__,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB_ERROR,
            "msg",              "%s", "hook field not found",
            "parent_node",      "%j", parent_node,
            "hook_name",        "%s", hook_name,
            NULL
        );
        return -1;
    }

    json_t *child_data = kw_get_dict_value(child_node, fkey_col_name, 0, 0);
    if(!child_data) {
        const char *child_topic_name = json_string_value(
            kwid_get("", child_node, "__md_treedb__`topic_name")
        );
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "fkey field not found in the node",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", fkey_col_name,
            NULL
        );
        log_debug_json(0, child_node, "fkey field not found in the node");
        return -1;
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
            const char *child_id = kw_get_str(child_node, "id", "", KW_REQUIRED);
            char pref[NAME_MAX];
            if(is_child_hook) {
                const char *child_topic_name = json_string_value(
                    kwid_get("", child_node, "__md_treedb__`topic_name")
                );
                snprintf(pref, sizeof(pref), "%s~%s~%s",
                    child_topic_name,
                    child_id,
                    fkey_col_name
                );
                json_object_set(parent_hook_data, pref, child_data);
            } else {
                json_object_set(parent_hook_data, child_id, child_node);
            }
        }
        break;
    default:
        {
            const char *parent_topic_name = json_string_value(
                kwid_get("", parent_node, "__md_treedb__`topic_name")
            );
            log_error(0,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                "msg",                  "%s", "Review the scheme: wrong parent hook type",
                "parent_topic_name",    "%s", parent_topic_name,
                "link",                 "%s", hook_name,
                NULL
            );
        }
        return -1;
    }

    return 0;
}


/***************************************************************************
 *  Load links (child's fkeys to parent's hooks)
 ***************************************************************************/
PRIVATE int load_links(
    json_t *tranger,
    json_t *child_node
)
{
    int ret = 0;

    const char *treedb_name = kw_get_str(child_node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(child_node, "__md_treedb__`topic_name", 0, KW_REQUIRED);

    json_t *cols = tranger_dict_topic_desc(
        tranger,
        topic_name
    );

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_child_hook = kw_has_word(desc_flag, "hook", "")?TRUE:FALSE;
        BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
        if(!is_fkey) {
            continue;
        }

        json_t *fkey_desc = kwid_get("", col, "fkey");
        if(!fkey_desc) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Child node without fkey field",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                "col_name",     "%s", col_name,
                NULL
            );
            log_debug_json(0, col, "Not explicit link done; child node without fkey field descriptor: col desc");
            log_debug_json(0, child_node, "Not explicit link done; child node without fkey field descriptor: node");
            ret += -1;
            continue;
        }

        /*
         *  Get fkeys from child_node fkey field
         */
        // **FKEY**
        json_t *fkeys = kw_get_dict_value(child_node, col_name, 0, 0);
        if(!fkeys) {
            continue;
        }
        switch(json_typeof(fkeys)) {
        case JSON_ARRAY:
            {
                int idx; json_t *fkey;
                json_array_foreach(fkeys, idx, fkey) {
                    const char *pref = json_string_value(fkey);

                    ret += link_child_to_parent(
                        tranger,
                        treedb_name,
                        pref,
                        child_node,
                        col_name,
                        fkey_desc,
                        is_child_hook
                    );
                }
            }
            break;
        case JSON_OBJECT:
            {
                const char *pref; json_t *fkey;
                json_object_foreach(fkeys, pref, fkey) {
                    ret += link_child_to_parent(
                        tranger,
                        treedb_name,
                        pref,
                        child_node,
                        col_name,
                        fkey_desc,
                        is_child_hook
                    );
                }
            }
            break;
        case JSON_STRING:
            {
                const char *pref = json_string_value(fkeys);

                ret += link_child_to_parent(
                    tranger,
                    treedb_name,
                    pref,
                    child_node,
                    col_name,
                    fkey_desc,
                    is_child_hook
                );
            }
            break;
        default:
            continue;
        }
    }

    json_decref(cols);
    return ret;
}

/***************************************************************************
 *  Load links from childs to parents  (child's fkeys to parent's hooks)
 ***************************************************************************/
PRIVATE int load_all_links(
    json_t *tranger,
    const char *treedb_name
)
{
    int ret = 0;

    /*
     *  Loop topics, as child nodes
     */
    json_t *topics = treedb_topics(tranger, treedb_name, "");
    int idx; json_t *jn_topic;
    json_array_foreach(topics, idx, jn_topic) {
        const char *topic_name = json_string_value(jn_topic);
        /*
         *  Loop nodes searching links
         */
        /*----------------------------------*
         *  Get indexx: to load links
         *----------------------------------*/
        json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
        if(!indexx) {
            // It's not a treedb topic
            continue;
        }

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
            ret += load_links(
                tranger,
                child_node
            );
        }
    }

    JSON_DECREF(topics);
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
    json_t *hook_data, // not owned
    BOOL original_node
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

                if(tildes == 2) {
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
                    log_debug_json(0, jn_value, "__md_treedb__ not found: value");
                    log_debug_json(0, hook_data, "__md_treedb__ not found: hook_data");
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
                                log_debug_json(0, jn_value, "__md_treedb__ not found: value");
                                log_debug_json(0, hook_data, "__md_treedb__ not found: hook_data");
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

                        if(tildes == 2) {
                            json_array_append(refs, jn_value);
                            break;
                        } else if(ampersands == 2) {
                            break;
                        } else if(ampersands == 1 && !original_node) {
                            // Can be a collapsed node
                            json_array_append(refs, jn_value);
                            break;
                        }
                    }
                default:
                    log_error(0,
                        "gobj",                 "%s", __FILE__,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "wrong array child hook type",
                        NULL
                    );
                    log_debug_json(0, jn_value, "wrong array child hook type: value");
                    log_debug_json(0, hook_data, "wrong array child hook type: hook data");
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
            if(count_char(json_string_value(field_data), '^')==2) {
                json_array_append(refs, field_data);
            }
        }
        break;
    case JSON_ARRAY:
        {
            int idx; json_t *ref;
            json_array_foreach(field_data, idx, ref) {
                if(json_typeof(ref)==JSON_STRING) {
                    if(count_char(json_string_value(ref), '^')==2) { // ==2 is 3 items!
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
PRIVATE json_t *get_node_down_refs(  // Return MUST be decref
    json_t *tranger,
    json_t *node    // not owned
)
{
    json_t *refs = json_array();

    const char *treedb_name = kw_get_str(node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(node, "__md_treedb__`topic_name", 0, KW_REQUIRED);
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);
    BOOL original_node = kw_get_bool(node, "__md_treedb__`__original_node__", 0, 0);

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_hook = kw_has_word(desc_flag, "hook", 0)?TRUE:FALSE;
        if(!is_hook) {
            continue;
        }

        json_t *field_data = kw_get_dict_value(node, col_name, 0, 0);
        if(!field_data) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "field not found in the node",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                "field",        "%s", col_name,
                NULL
            );
            log_debug_json(0, node, "field not found in the node");
        }
        if(json_empty(field_data)) {
            continue;
        }

        json_t *ref = get_hook_refs(field_data, original_node);
        json_array_extend(refs, ref);
        JSON_DECREF(ref);
    }

    JSON_DECREF(cols);
    return refs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_node_up_refs(  // Return MUST be decref
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
        if(!is_fkey) {
            continue;
        }

        json_t *field_data = kw_get_dict_value(node, col_name, 0, 0);
        if(!field_data) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "field not found in the node",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                "field",        "%s", col_name,
                NULL
            );
            log_debug_json(0, node, "field not found in the node");
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

/***************************************************************************
    Return refs of fkeys of col_name field
 ***************************************************************************/
PUBLIC json_t *treedb_node_up_refs(  // Return MUST be decref
    json_t *tranger,
    json_t *node,    // not owned
    const char *topic_name,
    const char *col_name,
    BOOL verbose
)
{
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);

    json_t *col = kw_get_dict_value(cols, col_name, 0, 0);
    if(!col) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "col_name not found in the desc",
            "topic_name",   "%s", topic_name,
            "field",        "%s", col_name,
            NULL
        );
        JSON_DECREF(cols);
        return 0;
    }
    json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
    BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
    if(!is_fkey) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field is not a fkey in the node",
            "topic_name",   "%s", topic_name,
            "col_name",     "%s", col_name,
            NULL
        );
        JSON_DECREF(cols);
        return 0;
    }

    json_t *refs = json_array();

    json_t *field_data = kw_get_dict_value(node, col_name, 0, 0);
    if(!field_data) {
        if(verbose) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "field not found in the node",
                "topic_name",   "%s", topic_name,
                "field",        "%s", col_name,
                NULL
            );
            log_debug_json(0, node, "field not found in the node");
        }
    }

    if(json_empty(field_data)) {
        JSON_DECREF(cols);
        return refs;
    }

    json_t *ref = get_fkey_refs(field_data);
    json_array_extend(refs, ref);
    JSON_DECREF(ref);

    JSON_DECREF(cols);
    return refs;
}

/***************************************************************************
    Return array of `parent_id` of `value`  (refs: parent_topic_name^parent_id^hook_name)
 ***************************************************************************/
PUBLIC json_t *treedb_beatiful_up_refs(  // Return MUST be decref
    json_t *value // not owned
)
{
    if(!value) {
        return 0;
    }
    char parent_topic_name[NAME_MAX];
    char parent_id[NAME_MAX];
    char hook_name[NAME_MAX];

    json_t *jn_beautiful_names = json_array();

    switch(json_typeof(value)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            if(decode_string_fkey(
                json_string_value(value),
                parent_topic_name, sizeof(parent_topic_name),
                parent_id, sizeof(parent_id),
                hook_name, sizeof(hook_name)
            )) {
                json_array_append_new(jn_beautiful_names, json_string(parent_id));
            }
        }
        break;
    case JSON_ARRAY:
        {
            int idx; json_t *r;
            json_array_foreach(value, idx, r) {
                if(json_typeof(r)==JSON_STRING) {
                    if(decode_string_fkey(
                        json_string_value(r),
                        parent_topic_name, sizeof(parent_topic_name),
                        parent_id, sizeof(parent_id),
                        hook_name, sizeof(hook_name)
                    )) {
                        json_array_append_new(jn_beautiful_names, json_string(parent_id));
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *key; json_t *v;
            json_object_foreach(value, key, v) {
                if(decode_string_fkey(
                    key,
                    parent_topic_name, sizeof(parent_topic_name),
                    parent_id, sizeof(parent_id),
                    hook_name, sizeof(hook_name)
                )) {
                    json_array_append_new(jn_beautiful_names, json_string(parent_id));
                }
            }
        }
        break;
    default:
        break;
    }

    return jn_beautiful_names;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int treedb_set_trace(BOOL set)
{
    BOOL old = treedb_trace;
    treedb_trace = set?1:0;
    return old;
}




                    /*------------------------------------*
                     *      Manage the tree's nodes
                     *------------------------------------*/




/***************************************************************************
 *  Copy inherit fieldss FROM primary_node TO secondary_node
 ***************************************************************************/
PRIVATE BOOL copy_inherit_fields(
    json_t *tranger,
    const char *topic_name,
    json_t *secondary_node, // not owned
    json_t *primary_node  // not owned
)
{
    BOOL ret = FALSE;

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
        return FALSE;
    }

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_inherit = kw_has_word(desc_flag, "inherit", 0)?TRUE:FALSE;
        if(!is_inherit) {
            continue;
        }

        /*
         *  Copy inherit field
         */
        json_t *cell = kw_get_dict_value(primary_node, col_name, 0, 0);
        if(cell) {
            json_object_set(secondary_node, col_name, cell);
            ret = TRUE;
        }
    }

    JSON_DECREF(cols);
    return ret;
}

/***************************************************************************
 *  Inherit links FROM primary_node TO secondary_node. In collapse mode.
 ***************************************************************************/
PRIVATE BOOL inherit_links(
    json_t *tranger,
    const char *topic_name,
    json_t *secondary_node, // not owned
    json_t *primary_node  // not owned
)
{
    BOOL ret = FALSE;

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
        return FALSE;
    }

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
        if(!is_fkey) {
            continue;
        }

        /*
         *  Copy fkeys
         */
        json_t *fkeys = treedb_node_up_refs(tranger, primary_node, topic_name, col_name, FALSE);
        json_t *cell = kw_get_dict_value(secondary_node, col_name, 0, KW_REQUIRED);
        if(cell) {
            int idx; json_t *fkey;
            json_array_foreach(fkeys, idx, fkey) {
                // **FKEY**
                const char *ref = json_string_value(fkey);
                if(json_is_array(cell)) {
                    json_array_append_new(cell, json_string(ref));
                    ret = TRUE;
                } else if(json_is_string(cell)) {
                    json_object_set_new(secondary_node, col_name, json_string(ref));
                    ret = TRUE;
                }
            }
        }

        json_decref(fkeys);
    }

    JSON_DECREF(cols);
    return ret;
}

/***************************************************************************
    WARNING This function does NOT auto build links
    If you want to create node with auto build links
    then you must use treedb_update_node() with "create"
 ***************************************************************************/
PUBLIC json_t *treedb_create_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw, // owned
    const char *options // "permissive" "verbose"
)
{
    BOOL verbose = (options && strstr(options, "verbose"))?TRUE:FALSE;

    /*-------------------------------*
     *  Get the id, it's mandatory
     *-------------------------------*/
    const char *id = kw_get_str(kw, "id", 0, 0);
    if(empty_string(id)) {
        json_t *id_col_flag = kwid_get("verbose",
            tranger,
            "topics`%s`cols`id`flag",
                topic_name
        );
        if(kw_has_word(id_col_flag, "uuid", 0)) {
            char uuid[RECORD_KEY_VALUE_MAX+1];
            uuid_t binuuid;
            uuid_generate_random(binuuid);
            uuid_unparse_lower(binuuid, uuid);
            id = uuid;
            json_object_set_new(kw, "id", json_string(id));
        } else if(kw_has_word(id_col_flag, "rowid", 0)) {
            json_int_t rowid = tranger_topic_size(tranger_topic(tranger, topic_name)) + 1;
            json_object_set_new(kw, "id", json_sprintf("%"JSON_INTEGER_FORMAT, rowid));
            id = kw_get_str(kw, "id", 0, 0);
        } else {
            if(verbose) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Field 'id' required",
                    "topic_name",   "%s", topic_name,
                    NULL
                );
                log_debug_json(0, kw, "Field 'id' required");
            }
            JSON_DECREF(kw);
            return 0;
        }
    }

    /*-------------------------------*
     *  Get indexx: to create node
     *-------------------------------*/
    json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
    if(!indexx) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexx NOT FOUND",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(kw);
        return 0;
    }

    /*-----------------------------------*
     *  Single: exists already the id?
     *-----------------------------------*/
    BOOL save_pkey = FALSE;
    BOOL save_id = FALSE;
    json_t *pkey2_list = json_array(); // collect pkeys to save

    json_t *prev_record = exist_primary_node(indexx, id);
    if(!prev_record) {
        save_id = TRUE;
    }
    /*-----------------------------------*
     *  Look for a secondary key change
     *-----------------------------------*/
    json_t *iter_pkey2s = treedb_topic_pkey2s(tranger, topic_name);
    const char *pkey2_name; json_t *jn_pkey2_fields;
    json_object_foreach(iter_pkey2s, pkey2_name, jn_pkey2_fields) {
        /*---------------------------------*
         *  Get indexy: to create node
         *---------------------------------*/
        json_t *indexy = treedb_get_pkey2_index(
            tranger,
            treedb_name,
            topic_name,
            pkey2_name
        );
        if(!indexy) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "TreeDb Topic indexy NOT FOUND",
                "topic_name",   "%s", topic_name,
                "pkey2_name",   "%s", pkey2_name,
                NULL
            );
            continue;
        }

        const char *pkey2_value = get_key2_value(
            tranger,
            topic_name,
            pkey2_name,
            kw
        );

        if(!kw_get_subdict_value(
                indexy,
                id,
                pkey2_value,
                0,
                KW_EMPTY_VALID
            )) {
            // Not exist
            save_pkey = TRUE;
            json_array_append_new(pkey2_list, json_string(pkey2_name));
        }
    }
    JSON_DECREF(iter_pkey2s);

    if(!save_id && !save_pkey) {
        log_warning(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Node already exists",
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(pkey2_list);
        JSON_DECREF(kw);
        return 0;
    }

    /*----------------------------------------*
     *  Create the tranger record to create
     *----------------------------------------*/
    json_t *record = record2tranger(tranger, topic_name, kw, options, TRUE);
    if(!record) {
        // Error already logged
        JSON_DECREF(pkey2_list);
        JSON_DECREF(kw);
        return 0;
    }
    BOOL links_inherited = FALSE;
    if(save_pkey) {
        /*
         *  Si es un nodo secundario, copia los links del primario.
         */
        links_inherited = inherit_links(tranger, topic_name, record, prev_record);

        /*
         *  Si es un nodo secundario, copia los inherit fields
         */
        copy_inherit_fields(tranger, topic_name, record, prev_record);
    }

    /*-------------------------------*
     *  Write to tranger (Creating)
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
        JSON_DECREF(pkey2_list);
        JSON_DECREF(kw);
        JSON_DECREF(record);
        return 0;
    }

    /*--------------------------------------------------*
     *  Set volatil data
     *  HACK set volatil after append record:
     *      Volatil data must not be save in file!
     *--------------------------------------------------*/
    set_volatil_values( // crea campos vacios o con los valores de kw
        tranger,
        topic_name,
        record,  // not owned
        kw // not owned
    );

    /*--------------------------------------------*
     *  Build metadata, creating node in memory
     *--------------------------------------------*/
    json_t *jn_record_md = _md2json(
        treedb_name,
        topic_name,
        &md_record
    );
    json_object_set_new(record, "__md_treedb__", jn_record_md);

    /*---------------------------------------------------*
     *  Si tienes la marca grupo, pasas, eres el activo.
     *  Si no tienes la marca,
     *  pasas a la cola secundaria de instancias,
     *  pasas a existir por la clave secundaria,
     *  si eres la última instancia claro.
     *  En este mundo sólo hay un activo,
     *  o el que tiene la marca de grupo,
     *  o el último en llegar si no hay marca de grupo.
     *  El resto son instancias,
     *  que viven por su identificación secundaria.
     *  En repetición de claves secundarias,
     *  la última es que prevalece y existe.
     *---------------------------------------------------*/

    /*-------------------------------*
     *  Build links to hooks
     *-------------------------------*/
    if(links_inherited) {
        load_links(tranger, record);
    }

    /*-------------------------------*
     *  Write node in memory: id
     *-------------------------------*/
    if(save_id) {
        add_primary_node(indexx, id, record);
    }

    /*-------------------------------*
     *  Write node in memory: pkey2
     *-------------------------------*/
    if(save_pkey) {
        int idx; json_t *jn_pkey2;
        json_array_foreach(pkey2_list, idx, jn_pkey2) {
            const char *pkey2_name = json_string_value(jn_pkey2);

            /*---------------------------------*
             *  Get indexy: to create node
             *---------------------------------*/
            json_t *indexy = treedb_get_pkey2_index(
                tranger,
                treedb_name,
                topic_name,
                pkey2_name
            );
            if(!indexy) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "TreeDb Topic indexy NOT FOUND",
                    "topic_name",   "%s", topic_name,
                    "pkey2_name",   "%s", pkey2_name,
                    NULL
                );
                continue;
            }
            const char *pkey2_value = get_key2_value(
                tranger,
                topic_name,
                pkey2_name,
                kw
            );
            kw_set_subdict_value(
                indexy,
                id,
                pkey2_value,
                json_incref(record)
            );
        }
    }

    /*-------------------------------*
     *  Trace
     *-------------------------------*/
    if(treedb_trace) {
        log_debug_json(0, record, "treedb_create_node: Ok (%s, %s)", treedb_name, topic_name);
    }

    json_decref(record);
    JSON_DECREF(pkey2_list);
    JSON_DECREF(kw);
    return record;
}

/***************************************************************************
 *  Direct saving to tranger.
    WARNING be care, must be a pure node.
    Tag __tag__ (user_flag) is inherited.
 ***************************************************************************/
PUBLIC int treedb_save_node(
    json_t *tranger,
    json_t *node    // not owned
)
{
    /*-------------------------------*
     *      Get record info
     *-------------------------------*/
    const char *treedb_name = kw_get_str(node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(node, "__md_treedb__`topic_name", 0, KW_REQUIRED);
    uint32_t tag = kw_get_int(node, "__md_treedb__`__tag__", 0, KW_REQUIRED);

    /*---------------------------------------*
     *  Create the tranger record to update
     *---------------------------------------*/
    json_t *record = record2tranger(tranger, topic_name, node, "", FALSE);
    if(!record) {
        // Error already logged
        return -1;
    }

    /*-------------------------------------*
     *  Write to tranger (save, updating)
     *-------------------------------------*/
    md_record_t md_record;
    int ret = tranger_append_record(
        tranger,
        topic_name,
        0, // __t__,         // if 0 then the time will be set by TimeRanger with now time
        tag, // user_flag,
        &md_record, // md_record,
        record // owned
    );
    if(ret < 0) {
        // Error already logged
        return -1;
    }

    /*----------------------------------*
     *  Build metadata, updating node
     *----------------------------------*/
    json_t *jn_record_md = _md2json(
        treedb_name,
        topic_name,
        &md_record
    );
    json_object_set_new(node, "__md_treedb__", jn_record_md);

    /*-------------------------------*
     *  Trace
     *-------------------------------*/
    if(treedb_trace) {
        log_debug_json(0, node, "treedb_save_node: Ok");
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int remove_wrong_up_ref(
    json_t *tranger,
    json_t *node,
    const char *topic_name,
    const char *col_name,
    const char *ref
)
{
    json_t *field_data = kw_get_dict_value(node, col_name, 0, 0);
    if(!field_data) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field not found in the node",
            "topic_name",   "%s", topic_name,
            "field",        "%s", col_name,
            NULL
        );
        log_debug_json(0, node, "field not found in the node");
    }

    if(json_empty(field_data)) {
        return -1;
    }

    int ret = -1;

    switch(json_typeof(field_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            const char *ref_ = json_string_value(field_data);
            if(ref_ && ref && strcmp(ref_, ref)==0) {
                kw_set_dict_value(node, col_name, json_string(""));
                ret = 0;
                log_warning(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Removing wrong fkey ref",
                    "topic_name",   "%s", topic_name,
                    "field",        "%s", col_name,
                    "ref",          "%s", ref,
                    NULL
                );
            }
        }
        break;
    case JSON_ARRAY:
        {
            int idx; json_t *r;
            json_array_foreach(field_data, idx, r) {
                if(json_typeof(r)==JSON_STRING) {
                    const char *ref_ = json_string_value(r);
                    if(ref_ && ref && strcmp(ref_, ref)==0) {
                        json_array_remove(field_data, idx);
                        idx --;
                        ret = 0;
                        log_warning(0,
                            "gobj",         "%s", __FILE__,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_TREEDB_ERROR,
                            "msg",          "%s", "Removing wrong fkey ref",
                            "topic_name",   "%s", topic_name,
                            "field",        "%s", col_name,
                            "ref",          "%s", ref,
                            NULL
                        );
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *ref_; json_t *v; void *n;
            json_object_foreach_safe(field_data, n, ref_, v) {
                if(ref_ && ref && strcmp(ref_, ref)==0) {
                    json_object_del(field_data, ref_);
                    ret = 0;
                    log_warning(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Removing wrong fkey ref",
                        "topic_name",   "%s", topic_name,
                        "field",        "%s", col_name,
                        "ref",          "%s", ref,
                        NULL
                    );
                }
            }
        }
        break;
    default:
        break;
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int search_and_remove_wrong_up_ref(
    json_t *tranger,
    json_t *node,
    const char *topic_name,
    const char *ref
)
{
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);
    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
        if(!is_fkey) {
            continue;
        }
        remove_wrong_up_ref(
            tranger,
            node,
            topic_name,
            col_name,
            ref
        );
    }
    json_decref(cols);
    return 0;
}

/***************************************************************************
    This function DOES auto build links

    "create" ["permissive"] create node if not exist
    "clean" clean wrong fkeys
 ***************************************************************************/
PUBLIC json_t *treedb_update_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,    // owned (node)
    const char *options // "create" ["permissive"], "clean"
)
{
    BOOL create = (options && strstr(options, "create"))?TRUE:FALSE;

    /*-------------------------------*
     *  Get id
     *-------------------------------*/
    const char *id = kw_get_str(kw, "id", "", 0);
    if(empty_string(id)) {
        if(!create) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "kw without id",
                NULL
            );
            JSON_DECREF(kw);
            return 0;
        }
    }

    /*-------------------------------*
     *  Set node name
     *-------------------------------*/
    char node_name[NAME_MAX]; // ONLY to log
    snprintf(node_name, sizeof(node_name), "%s^%s", topic_name, id?id:"");

    /*-------------------------------*
     *  Recover node
     *-------------------------------*/
    json_t *node = 0;
    if(!empty_string(id)) {
        node = treedb_get_node( // Return is NOT YOURS
            tranger,
            treedb_name,
            topic_name,
            id,
            0
        );
    }
    if(!node) {
        if(options && strstr(options, "create")) {
            JSON_INCREF(kw);
            node = treedb_create_node(
                tranger,
                treedb_name,
                topic_name,
                kw, // owned
                options
            );
            if(!node) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Cannot create node ",
                    "treedb_name",  "%s", treedb_name,
                    "topic_name",   "%s", topic_name,
                    "id",           "%s", id,
                    NULL
                );
                JSON_DECREF(kw);
                return 0;
            }
        } else {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "node not found",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
            JSON_DECREF(kw);
            return 0;
        }
    }

    /*-------------------------------*
     *  Update fields
     *-------------------------------*/
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
        JSON_DECREF(kw);
        return 0;
    }

    char parent_topic_name[NAME_MAX];
    char parent_id[NAME_MAX];
    char hook_name[NAME_MAX];

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
        if(!is_fkey) {
            BOOL is_hook = kw_has_word(desc_flag, "hook", 0)?TRUE:FALSE;
            if(!is_hook) {
                /*
                 *  Check if has changed
                 */
                json_t *old_value = kw_get_dict_value(node, col_name, 0, 0);
                json_t *new_value = kw_get_dict_value(kw, col_name, 0, 0);
                if(new_value && !kw_is_identical(old_value, new_value)) {
                    json_object_set(node, col_name, new_value);
                }
            }
            continue;
        }

        /*
         *  link/unlink fkeys
         */
        json_t *old_fkeys = treedb_node_up_refs(tranger, node, topic_name, col_name, TRUE);
        json_t *new_fkeys = treedb_node_up_refs(tranger, kw, topic_name, col_name, TRUE);

        int idx; json_t *new_fkey;
        json_array_foreach(new_fkeys, idx, new_fkey) {
            // **FKEY**
            const char *ref = json_string_value(new_fkey);
            int old_idx = json_list_str_index(old_fkeys, ref, FALSE);
            if(old_idx >= 0) {
                json_array_remove(old_fkeys, old_idx);
                continue;
            }

            /*
             *  New link
             */

            /*
             *  Get parent info
             */
            if(!decode_string_fkey(
                ref,
                parent_topic_name, sizeof(parent_topic_name),
                parent_id, sizeof(parent_id),
                hook_name, sizeof(hook_name)
            )) {
                // It's not a fkey
                log_error(0,
                    "gobj",                 "%s", __FILE__,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "Wrong parent reference: must be \"parent_topic_name^parent_id^hook_name\"",
                    "parent",               "%s", ref,
                    "child",                "%s", node_name,
                    NULL
                );
                continue;
            }

            json_t *parent_node = treedb_get_node( // Return is NOT YOURS
                tranger,
                treedb_name,
                parent_topic_name,
                parent_id,
                0
            );
            if(!parent_node) {
                log_error(0,
                    "gobj",                 "%s", __FILE__,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "update_node, new link: parent node not found",
                    "parent",               "%s", ref,
                    "child",                "%s", node_name,
                    NULL
                );
                continue;
            }

            _link_nodes(
                tranger,
                hook_name,
                parent_node,    // not owned
                node      // not owned
            );
        }

        json_t *old_fkey;
        json_array_foreach(old_fkeys, idx, old_fkey) {
            /*
             *  Delete link
             */
            const char *ref = json_string_value(old_fkey);

            /*
             *  Get parent info
             */
            if(!decode_string_fkey(
                ref,
                parent_topic_name, sizeof(parent_topic_name),
                parent_id, sizeof(parent_id),
                hook_name, sizeof(hook_name)
            )) {
                // It's not a fkey
                log_error(0,
                    "gobj",                 "%s", __FILE__,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "Wrong parent reference: must be \"parent_topic_name^parent_id^hook_name\"",
                    "parent",               "%s", ref,
                    NULL
                );
                continue;
            }

            json_t *parent_node = treedb_get_node( // Return is NOT YOURS
                tranger,
                treedb_name,
                parent_topic_name,
                parent_id,
                0
            );
            if(!parent_node) {
                log_error(0,
                    "gobj",                 "%s", __FILE__,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "update_node, delete link: parent node not found",
                    "parent",               "%s", ref,
                    "child",                "%s", node_name,
                    NULL
                );
                if(options && strstr(options, "clean")) {
                    /*
                     *  Remove reference, it's invalid
                     */
                    remove_wrong_up_ref(
                        tranger,
                        node,
                        topic_name,
                        col_name,
                        ref
                    );
                }
                continue;
            }

            _unlink_nodes(
                tranger,
                hook_name,
                parent_node,    // not owned
                node      // not owned
            );
        }

        json_decref(old_fkeys);
        json_decref(new_fkeys);
    }

    if(options && strstr(options, "permissive")) {
        json_object_update_missing(node, kw);
    }

    JSON_DECREF(cols);
    JSON_DECREF(kw);

    /*-------------------------------*
     *  Write to tranger
     *-------------------------------*/
    if(treedb_save_node(tranger, node)<0) {
        // Error already logged
        return 0;
    }
    return node;
}

/***************************************************************************
    "force" delete links.
    If there are links and not force then delete_node will fail
    WARNING that kw can be node, the node to delete!!

    HACK: delete will be delete the record forever, for that reason,
          a node with snap tag cannot be delete!

 ***************************************************************************/
PUBLIC int treedb_delete_node(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,    // owned (node, WARNING only 'id' field is used to find the node to delete)
    const char *options // "force"
)
{
    /*-------------------------------*
     *  Get id
     *-------------------------------*/
    const char *id = kw_get_str(kw, "id", 0, 0);
    if(empty_string(id)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "kw without id",
            NULL
        );
        log_debug_json(0, kw, "kw without id");
        JSON_DECREF(kw);
        return -1;
    }

    /*-------------------------------*
     *  Set node name
     *-------------------------------*/
    char node_name[NAME_MAX];
    snprintf(node_name, sizeof(node_name), "%s^%s", topic_name, id);

    /*-------------------------------*
     *  Recover node
     *-------------------------------*/
    json_t *node = treedb_get_node( // Return is NOT YOURS
        tranger,
        treedb_name,
        topic_name,
        id,
        0
    );
    if(!node) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "node not found",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(kw);
        return -1;
    }
    if(kw != node) {
        JSON_DECREF(kw);
    }

    /*-------------------------------*
     *      Get record info
     *-------------------------------*/
    json_int_t __rowid__ = kw_get_int(node, "__md_treedb__`__rowid__", 0, KW_REQUIRED);
    json_int_t __tag__ = kw_get_int(node, "__md_treedb__`__tag__", 0, KW_REQUIRED);
    if(__tag__) {
        // TODO añade opción de borrar un snap que desmarque los nodos
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "cannot delete node, it has a tag",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(kw);
        return -1;
    }

    /*-------------------------------*
     *  Get indexx: to delete node
     *-------------------------------*/
    json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
    if(!indexx) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexx NOT FOUND",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return -1;
    }

    /*-------------------------------*
     *  Check hooks and fkeys
     *-------------------------------*/
    BOOL to_delete = TRUE;

    json_t *down_refs = get_node_down_refs(tranger, node);
    if(json_array_size(down_refs)>0) {
        if(options && strstr(options, "force")) {
            json_t *jn_hooks = treedb_get_topic_hooks(
                tranger,
                treedb_name,
                topic_name
            );
            int idx; json_t *jn_hook;
            json_array_foreach(jn_hooks, idx, jn_hook) {
                const char *hook = json_string_value(jn_hook);
                json_t *childs = treedb_list_childs(tranger, hook, node, 0);
                int idx; json_t *child;
                json_array_foreach(childs, idx, child) {
                    treedb_unlink_nodes(tranger, hook, node, child);
                }
                JSON_DECREF(childs);
            }
            JSON_DECREF(jn_hooks);

        } else {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot delete node: has down links",
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
        }
    }
    JSON_DECREF(down_refs);

    json_t *up_refs = get_node_up_refs(tranger, node);
    if(json_array_size(up_refs)>0) {
        if(options && strstr(options, "force")) {
            int idx; json_t *old_fkey;
            json_array_foreach(up_refs, idx, old_fkey) {
                /*
                 *  Delete link
                 */
                const char *ref = json_string_value(old_fkey);

                /*
                 *  Get parent info
                 */
                char parent_topic_name[NAME_MAX];
                char parent_id[NAME_MAX];
                char hook_name[NAME_MAX];
                if(!decode_string_fkey(
                    ref,
                    parent_topic_name, sizeof(parent_topic_name),
                    parent_id, sizeof(parent_id),
                    hook_name, sizeof(hook_name)
                )) {
                    // It's not a fkey
                    log_error(0,
                        "gobj",                 "%s", __FILE__,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "Wrong parent reference: must be \"parent_topic_name^parent_id^hook_name\"",
                        "ref",                  "%s", ref,
                        NULL
                    );
                    continue;
                }

                json_t *parent_node = treedb_get_node( // Return is NOT YOURS
                    tranger,
                    treedb_name,
                    parent_topic_name,
                    parent_id,
                    0
                );
                if(parent_node) {
                    _unlink_nodes(
                        tranger,
                        hook_name,
                        parent_node,    // not owned
                        node      // not owned
                    );
                } else {
                    search_and_remove_wrong_up_ref(
                        tranger,
                        node,
                        topic_name,
                        ref
                    );
                }
            }

            /*
             *  Re-checks up links
             */
            json_t *up_refs_ = get_node_up_refs(tranger, node);
            if(json_array_size(up_refs_)>0) {
                to_delete = FALSE;
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Cannot delete node: still has up links",
                    "topic_name",   "%s", topic_name,
                    "id",           "%s", id,
                    NULL
                );
            }
            JSON_DECREF(up_refs_);

        } else {
            to_delete = FALSE;
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot delete node: has up links",
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
        }
    }
    JSON_DECREF(up_refs);

    if(!to_delete) {
        // Error already logged
        JSON_DECREF(node);
        return -1;
    }

    /*-------------------------------------------------*
     *  Delete the record
     *  HACK cannot use tranger_delete_record()
     *  List of deleted id's in memory
     *  (borrar un id record en tranger, y el resto?)
     *-------------------------------------------------*/
    if(tranger_write_mark1(tranger, topic_name, __rowid__, TRUE)==0) {
        BOOL ok_deleted = TRUE;

        /*-------------------------------*
         *      Borra indexx data
         *-------------------------------*/
        if(delete_primary_node(indexx, id)<0) { // node owned
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "json_object_del() FAILED",
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
            ok_deleted = FALSE;
        }

        /*-------------------------------*
         *      Borra indexy data
         *-------------------------------*/
        json_t *iter_pkey2s = treedb_topic_pkey2s(tranger, topic_name);
        const char *pkey2_name; json_t *jn_pkey2_fields;
        json_object_foreach(iter_pkey2s, pkey2_name, jn_pkey2_fields) {
            /*---------------------------------*
             *  Get indexy: to delete node
             *---------------------------------*/
            json_t *indexy = treedb_get_pkey2_index(
                tranger,
                treedb_name,
                topic_name,
                pkey2_name
            );
            if(!indexy) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "TreeDb Topic indexy NOT FOUND",
                    "topic_name",   "%s", topic_name,
                    "pkey2_name",   "%s", pkey2_name,
                    NULL
                );
                ok_deleted = FALSE;
                continue;
            }

            json_t * key2v = kw_get_dict_value(
                indexy,
                id,
                0,
                KW_EXTRACT
            );
            if(key2v) {
                json_decref(key2v);
            } else {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "delete pkey2 FAILED",
                    "topic_name",   "%s", topic_name,
                    "id",           "%s", id,
                    "pkey2_name",   "%s", pkey2_name,
                    NULL
                );
                ok_deleted = FALSE;
            }
        }
        JSON_DECREF(iter_pkey2s);

        if(ok_deleted) {
            /*-------------------------------*
             *  Trace
             *-------------------------------*/
            if(treedb_trace) {
                trace_msg("json_object_del() Ok, topic %s, id %s", topic_name, id);
            }
        }

    } else {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot delete node",
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(node);
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _link_nodes(
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
)
{
    /*---------------------------------------*
     *  Check if original nodes
     *---------------------------------------*/
    if(!kw_get_bool(parent_node, "__md_treedb__`__original_node__", 0, 0)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot link not original parent node",
            "parent_node",  "%j", parent_node,
            NULL
        );
        return -1;
    }
    if(!kw_get_bool(child_node, "__md_treedb__`__original_node__", 0, 0)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot link not original child node",
            "parent_node",  "%j", parent_node,
            NULL
        );
        return -1;
    }

    /*---------------------------------------*
     *  Check self link
     *---------------------------------------*/
    if(parent_node == child_node) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot link self node",
            "parent_node",  "%j", parent_node,
            NULL
        );
        return -1;
    }

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
            "hook_desc",    "%j", hook_desc,
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
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field not found in the node",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
        log_debug_json(0, child_node, "field not found in the node");
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
     *      _link_nodes() and link_child_to_parent()
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

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _unlink_nodes(
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
)
{
    /*---------------------------------------*
     *  Check if original nodes
     *---------------------------------------*/
    if(!kw_get_bool(parent_node, "__md_treedb__`__original_node__", 0, 0)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot link not original parent node",
            "parent_node",  "%j", parent_node,
            NULL
        );
        return -1;
    }
    if(!kw_get_bool(child_node, "__md_treedb__`__original_node__", 0, 0)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot unlink not original child node",
            "parent_node",  "%j", parent_node,
            NULL
        );
        return -1;
    }

    /*---------------------------------------*
     *  Check self link
     *---------------------------------------*/
    if(parent_node == child_node) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot unlink self node",
            "parent_node",  "%j", parent_node,
            NULL
        );
        return -1;
    }

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
            "hook_desc",    "%j", hook_desc,
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
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field not found in the node",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
        log_debug_json(0, child_node, "field not found in the node");
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

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int link_or_unlink_nodes2(
    json_t *tranger,
    const char *treedb_name,
    const char *parent,     // parent_topic_name^parent_id^hook_name
    const char *child,      // child_topic_name^child_id
    int (*link_or_unlink_fn)(
        json_t *tranger,
        const char *hook,
        json_t *parent_node,    // not owned
        json_t *child_node      // not owned
    )
)
{
    if(empty_string(parent)) {
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "Parent ref is empty",
            NULL
        );
        return -1;
    }
    if(empty_string(child)) {
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "Child ref is empty",
            NULL
        );
        return -1;
    }

    /*--------------------------------*
     *      Get parent info
     *--------------------------------*/
    char parent_topic_name[NAME_MAX];
    char parent_id[NAME_MAX];
    char hook_name[NAME_MAX];
    if(!decode_string_fkey(
        parent,
        parent_topic_name, sizeof(parent_topic_name),
        parent_id, sizeof(parent_id),
        hook_name, sizeof(hook_name)
    )) {
        // It's not a fkey
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "Wrong parent reference: must be \"parent_topic_name^parent_id^hook_name\"",
            "parent",               "%s", parent,
            "child",                "%s", child,
            NULL
        );
        return -1;
    }

    json_t *parent_node = treedb_get_node( // Return is NOT YOURS
        tranger,
        treedb_name,
        parent_topic_name,
        parent_id,
        0
    );
    if(!parent_node) {
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "Parent node not found",
            "parent",               "%s", parent,
            NULL
        );
        return -1;
    }

    /*--------------------------------*
     *      Get child info
     *--------------------------------*/
    char child_topic_name[NAME_MAX];
    char child_id[NAME_MAX];
    if(!decode_child_ref(
        child,
        child_topic_name, sizeof(child_topic_name),
        child_id, sizeof(child_id)
    )) {
        // It's not a child ref
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "Wrong child reference: must be \"child_topic_name^child_id\"",
            "child",               "%s", child,
            "child",                "%s", child,
            NULL
        );
        return -1;
    }

    json_t *child_node = 0;

    if(child_id[0]=='$') {
        int lsz_func;
        const char **func_param = split2(child_id+1, "$", &lsz_func);
        if(!func_param || lsz_func!=2) {
            split_free2(func_param);
            log_error(0,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                "msg",                  "%s", "Wrong child reference: must be \"child_topic_name^child_id\"",
                "parent",               "%s", parent,
                "child",                "%s", child,
                NULL
            );
            return -1;
        }

        const char *f = func_param[0];
        const char *param = func_param[1];
        json_t *jn_param = legalstring2json(param, TRUE);
        if(!jn_param) {
            split_free2(func_param);
            log_error(0,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                "msg",                  "%s", "Wrong function parameters",
                "parent",               "%s", parent,
                "child",                "%s", child,
                NULL
            );
            return -1;
        }
        SWITCHS(f) {
            CASES("find-node")
                json_t *nodes = treedb_list_nodes( // Return MUST be decref
                    tranger,
                    treedb_name,
                    child_topic_name,
                    jn_param,  // owned
                    0,  // options, "collapsed"
                    0   //match_fn
                );
                if(json_array_size(nodes)!=1) {
                    split_free2(func_param);
                    log_error(0,
                        "gobj",                 "%s", __FILE__,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "Filter must match only one node",
                        "parent",               "%s", parent,
                        "child",                "%s", child,
                        NULL
                    );
                    return -1;
                }
                child_node = kw_get_list_value(nodes, 0, 0);
                JSON_DECREF(nodes);
                split_free2(func_param);
                break;

            DEFAULTS
                split_free2(func_param);
                log_error(0,
                    "gobj",                 "%s", __FILE__,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "Function unknown",
                    "parent",               "%s", parent,
                    "child",                "%s", child,
                    "f",                    "%s", f,
                    NULL
                );
                return -1;
                break;
        } SWITCHS_END;
    } else {
        child_node = treedb_get_node( // Return is NOT YOURS
            tranger,
            treedb_name,
            child_topic_name,
            child_id,
            0
        );
    }

    if(!child_node) {
        log_error(0,
            "gobj",                 "%s", __FILE__,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "Child node not found",
            "parent",               "%s", parent,
            "child",                "%s", child,
            NULL
        );
        return -1;
    }

    int result = link_or_unlink_fn(
        tranger,
        hook_name,
        parent_node,    // not owned
        child_node      // not owned
    );

    return result;
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
    if(_link_nodes(
        tranger,
        hook_name,
        parent_node,    // not owned
        child_node      // not owned
    ) < 0) {
        // Error already logged
        return -1;
    }

    /*----------------------------*
     *      Save persistents
     *  Only childs are saved
     *----------------------------*/
    return treedb_save_node(tranger, child_node);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_link_nodes2(
    json_t *tranger,
    const char *treedb_name,
    const char *parent,     // parent_topic_name^parent_id^hook_name
    const char *child       // child_topic_name^child_id
)
{
    return link_or_unlink_nodes2(
        tranger,
        treedb_name,
        parent,
        child,
        treedb_link_nodes
    );
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
    if(_unlink_nodes(
        tranger,
        hook_name,
        parent_node,    // not owned
        child_node      // not owned
    ) < 0) {
        // Error already logged
        return -1;
    }

    /*----------------------------*
     *      Save persistents
     *  Only childs are saved
     *----------------------------*/
    return treedb_save_node(tranger, child_node);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_unlink_nodes2(
    json_t *tranger,
    const char *treedb_name,
    const char *parent,     // parent_topic_name^parent_id^hook_name
    const char *child       // child_topic_name^child_id
)
{
    return link_or_unlink_nodes2(
        tranger,
        treedb_name,
        parent,
        child,
        treedb_unlink_nodes
    );
}

/***************************************************************************
    Return a view of node with hook fields being collapsed
    WARNING extra fields are ignored, only topic desc fields are used
 ***************************************************************************/
PRIVATE json_t *node_collapsed_view( // Return MUST be decref
    json_t *topic_desc, // not owned
    json_t *node // not owned
)
{
    BOOL original_node = kw_get_bool(node, "__md_treedb__`__original_node__", 0, 0);
    json_t *node_view = json_object();

    const char *col_name; json_t *col;
    json_object_foreach(topic_desc, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_hook = kw_has_word(desc_flag, "hook", 0)?TRUE:FALSE;
        BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
        BOOL is_required = kw_has_word(desc_flag, "required", 0)?TRUE:FALSE;
        json_t *field_value = kw_get_dict_value(node, col_name, 0, is_required?KW_REQUIRED:0);
        if(!field_value) {
            // Something wrong
            json_object_set_new(
                node_view,
                col_name,
                json_null()
            );
            continue;
        }
        if(is_hook || is_fkey) {
            if(is_hook) {
                json_t *list = kw_get_dict_value(
                    node_view,
                    col_name,
                    json_array(),
                    KW_CREATE
                );
                json_t *hook_refs = get_hook_refs(field_value, original_node);
                json_array_extend(list, hook_refs);
                json_decref(hook_refs);
            }
            if(is_fkey) {
                json_t *list = kw_get_dict_value(
                    node_view,
                    col_name,
                    json_array(),
                    KW_CREATE
                );
                json_t *fkey_refs = get_fkey_refs(field_value);
                json_array_extend(list, fkey_refs);
                json_decref(fkey_refs);
            }
        } else {
            json_object_set_new(
                node_view,
                col_name,
                json_deep_copy(field_value)
            );
        }
    }
    json_object_set_new(
        node_view,
        "__md_treedb__",
        kw_duplicate(json_object_get(node, "__md_treedb__"))
    );
    json_object_set_new(
        json_object_get(node_view, "__md_treedb__"),
        "__original_node__",
        json_false()
    );
    return node_view;
}

/***************************************************************************
    Return a list of matched nodes
    If collapsed:
        - the ref (fkeys to up) have 3 ^ fields
        - the ref (childs, to down) have 2 ^ fields
    WARNING Returned list in "collapsed" mode have duplicate nodes,
            otherwise it returns original nodes.

    HACK id is converted in ids (using kwid_get_ids())
    HACK if __filter__ exists in jn_filter it will be used as filter

 ***************************************************************************/
PUBLIC json_t *treedb_list_nodes( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_filter_,  // owned
    json_t *jn_options, // owned, "collapsed"
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    /*-----------------------------------*
     *  Use duplicate, will be modified
     *-----------------------------------*/
    json_t *jn_filter = jn_filter_?kw_duplicate(jn_filter_):0;
    JSON_DECREF(jn_filter_);

    /*-------------------------------*
     *  Get indexx: to list nodes
     *-------------------------------*/
    json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
    if(!indexx) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexx NOT FOUND",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_options);
        JSON_DECREF(jn_filter);
        return 0;
    }

    /*--------------------------------------------*
     *      Search
     *  Extract from jn_filter
     *      the ids and the fields of topic
     *--------------------------------------------*/
    if(!match_fn) {
        match_fn = kw_match_simple;
    }

    /*
     *  Extrae ids
     */
    json_t *ids_list = 0;
    json_t *jn_id = kw_get_dict_value(jn_filter, "id", 0, KW_EXTRACT|KW_DONT_LOG);
    if(jn_id) {
        ids_list = kwid_get_ids(jn_id);
        JSON_DECREF(jn_id);
    }

    /*
     *  Usa __filter__ si existe
     */
    if(kw_has_key(jn_filter, "__filter__")) {
        json_t *jn_filter_ = kw_get_dict_value(jn_filter, "__filter__", 0, KW_EXTRACT);
        JSON_DECREF(jn_filter);
        jn_filter = jn_filter_;
    }

    /*
     *  Filtra de jn_filter solo las keys del topic
     */
    json_t *topic_desc = tranger_dict_topic_desc(tranger, topic_name);
    jn_filter = kw_clone_by_keys(
        jn_filter,     // owned
        kw_incref(topic_desc), // owned
        FALSE
    );

    BOOL collapsed = kw_get_bool(jn_options, "collapsed", 0, KW_WILD_NUMBER);

    json_t *list = json_array();

    if(json_is_array(indexx)) {
        size_t idx; json_t *node;
        json_array_foreach(indexx, idx, node) {
            if(!kwid_match_id(ids_list, kw_get_str(node, "id", 0, 0))) {
                continue;
            }
            JSON_INCREF(jn_filter);
            if(match_fn(node, jn_filter)) {
                if(!collapsed) {
                    // Full tree
                    json_array_append(list, node);
                } else {
                    // Collapse records (hook fields)
                    json_array_append_new(
                        list,
                        node_collapsed_view(
                            topic_desc,
                            node
                        )
                    );
                }
            }
        }
    } else if(json_is_object(indexx)) {
        const char *id; json_t *node;
        json_object_foreach(indexx, id, node) {
            if(!kwid_match_id(ids_list, id)) {
                continue;
            }
            JSON_INCREF(jn_filter);
            if(match_fn(node, jn_filter)) {
                if(!collapsed) {
                    json_array_append(list, node);
                } else {
                    json_array_append_new(
                        list,
                        node_collapsed_view(
                            topic_desc,
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

    JSON_DECREF(topic_desc);
    JSON_DECREF(jn_filter);
    JSON_DECREF(ids_list);
    JSON_DECREF(jn_options);

    return list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_node_instances( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name,
    json_t *jn_filter_,  // owned
    json_t *jn_options, // owned, "collapsed"
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    /*-----------------------------------*
     *  Use duplicate, will be modified
     *-----------------------------------*/
    json_t *jn_filter = jn_filter_?kw_duplicate(jn_filter_):0;
    JSON_DECREF(jn_filter_);

    /*--------------------------------------------*
     *      Search
     *  Extract from jn_filter
     *      the ids and the fields of topic
     *--------------------------------------------*/
    if(!match_fn) {
        match_fn = kw_match_simple;
    }

    /*
     *  Extrae ids
     */
    json_t *ids_list = 0;
    json_t *jn_id = kw_get_dict_value(jn_filter, "id", 0, KW_EXTRACT|KW_DONT_LOG);
    if(jn_id) {
        ids_list = kwid_get_ids(jn_id);
        JSON_DECREF(jn_id);
    }

    /*
     *  Usa __filter__ si existe
     */
    if(kw_has_key(jn_filter, "__filter__")) {
        json_t *jn_filter_ = kw_get_dict_value(jn_filter, "__filter__", 0, KW_EXTRACT);
        JSON_DECREF(jn_filter);
        jn_filter = jn_filter_;
    }

    /*
     *  Filtra de jn_filter solo las keys del topic
     */
    json_t *topic_desc = tranger_dict_topic_desc(tranger, topic_name);
    jn_filter = kw_clone_by_keys(
        jn_filter,     // owned
        kw_incref(topic_desc), // owned
        FALSE
    );

    BOOL collapsed = kw_get_bool(jn_options, "collapsed", 0, KW_WILD_NUMBER);

    /*-------------------------------*
     *      Create list
     *-------------------------------*/
    json_t *list = json_array();

    /*-------------------------------*
     *      Get indexy's
     *-------------------------------*/
    json_t *iter_pkey2s = 0;
    if(empty_string(pkey2_name)) {
        iter_pkey2s = treedb_topic_pkey2s(tranger, topic_name);
    } else {
        iter_pkey2s = json_object();
        json_object_set_new(iter_pkey2s, pkey2_name, json_string(pkey2_name));
    }

    json_t *pkey2_fields;
    json_object_foreach(iter_pkey2s, pkey2_name, pkey2_fields) {
        /*---------------------------------*
         *  Get indexy: to list instances
         *---------------------------------*/
        json_t *indexy = treedb_get_pkey2_index(
            tranger,
            treedb_name,
            topic_name,
            pkey2_name
        );
        if(!indexy) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "TreeDb Topic indexy NOT FOUND",
                "topic_name",   "%s", topic_name,
                "pkey2_name",   "%s", pkey2_name,
                NULL
            );
            continue;
        }

        if(json_is_object(indexy)) {
            const char *id; json_t *pkey2_dict;
            json_object_foreach(indexy, id, pkey2_dict) {
                if(!kwid_match_id(ids_list, id)) {
                    continue;
                }
                const char *pkey2; json_t *node;
                json_object_foreach(pkey2_dict, pkey2, node) {
                    JSON_INCREF(jn_filter);
                    if(match_fn(node, jn_filter)) {
                        if(!collapsed) {
                            json_array_append(list, node);
                        } else {
                            json_array_append_new(
                                list,
                                node_collapsed_view(
                                    topic_desc,
                                    node
                                )
                            );
                        }
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
        }
    }

    JSON_DECREF(iter_pkey2s);
    JSON_DECREF(topic_desc);
    JSON_DECREF(ids_list);
    JSON_DECREF(jn_filter);
    JSON_DECREF(jn_options);

    return list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_get_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *id,
    json_t *jn_options // owned, "collapsed"
)
{
    /*-------------------------------*
     *  Get indexx: to get node
     *-------------------------------*/
    json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
    if(!indexx) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexx NOT FOUND",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_options);
        return 0;
    }

    /*-------------------------------*
     *      Get
     *-------------------------------*/
    BOOL collapsed = kw_get_bool(jn_options, "collapsed", 0, KW_WILD_NUMBER);
    json_t *node = exist_primary_node(indexx, id);
    if(!node) {
        // Silence
        JSON_DECREF(jn_options);
        return 0;
    }
    if(collapsed) {
        node = treedb_collapse_node(tranger, node);
    }

    JSON_DECREF(jn_options);
    return node;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_collapse_node( // Return MUST be decref
    json_t *tranger,
    json_t *node // not owned
)
{
    const char *topic_name = json_string_value(
        kwid_get("", node, "__md_treedb__`topic_name")
    );
    json_t *topic_desc = tranger_dict_topic_desc(tranger, topic_name);
    json_t *collapsed = node_collapsed_view(
        topic_desc,
        node
    );
    JSON_DECREF(topic_desc);
    return collapsed;
}

/***************************************************************************
 *  Return a list of parent nodes pointed by the link (fkey)
 ***************************************************************************/
PUBLIC json_t *treedb_list_parents( // Return MUST be decref
    json_t *tranger,
    const char *link, // must be a fkey field
    json_t *node, // not owned
    json_t *jn_options // owned, "collapsed"
)
{
    const char *treedb_name = kw_get_str(node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(node, "__md_treedb__`topic_name", 0, KW_REQUIRED);
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);
    BOOL collapsed = kw_get_bool(jn_options, "collapsed", 0, KW_WILD_NUMBER);

    json_t *col = kw_get_dict_value(cols, link, 0, 0);
    if(!col) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "link not found in the desc",
            "topic_name",   "%s", topic_name,
            "link",         "%s", link,
            NULL
        );
        log_debug_json(0, node, "link not found in the desc");
        JSON_DECREF(jn_options);
        JSON_DECREF(cols);
        return 0;
    }
    json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
    BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
    if(!is_fkey) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "link is not a fkey in the node",
            "topic_name",   "%s", topic_name,
            "link",         "%s", link,
            NULL
        );
        log_debug_json(0, node, "link is not a fkey in the node");
        JSON_DECREF(jn_options);
        JSON_DECREF(cols);
        return 0;
    }

    json_t *field_data = kw_get_dict_value(node, link, 0, 0);
    if(!field_data) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "link not found in the node",
            "topic_name",   "%s", topic_name,
            "link",         "%s", link,
            NULL
        );
        log_debug_json(0, node, "link not found in the node");
    }

    if(json_empty(field_data)) {
        JSON_DECREF(jn_options);
        JSON_DECREF(cols);
        return json_array();
    }

    json_t *refs = get_fkey_refs(field_data);
    if(collapsed) {
        JSON_DECREF(jn_options);
        JSON_DECREF(cols);
        return refs;
    }

    json_t *parents = json_array();
    int idx; json_t *jn_fkey;
    json_array_foreach(refs, idx, jn_fkey) {
        char parent_topic_name[NAME_MAX];
        char parent_id[NAME_MAX];
        char hook_name[NAME_MAX];
        /*
         *  Get parent info
         */
        const char *ref = json_string_value(jn_fkey);
        if(!decode_string_fkey(
            ref,
            parent_topic_name, sizeof(parent_topic_name),
            parent_id, sizeof(parent_id),
            hook_name, sizeof(hook_name)
        )) {
            // It's not a fkey
            log_error(0,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                "msg",                  "%s", "Wrong parent reference: must be \"parent_topic_name^parent_id^hook_name\"",
                "parent",               "%s", ref,
                "topic_name",                "%s", topic_name,
                NULL
            );
            log_debug_json(0, node, "Wrong parent reference:");
            continue;
        }

        json_t *parent_node = treedb_get_node( // Return is NOT YOURS
            tranger,
            treedb_name,
            parent_topic_name,
            parent_id,
            0
        );
        if(!parent_node) {
            log_error(0,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                "msg",                  "%s", "get_parent_nodes: parent node not found",
                "parent",               "%s", ref,
                "topic_name",           "%s", topic_name,
                NULL
            );
            log_debug_json(0, node, "get_parent_nodes: parent node not found");
            continue;
        }

        json_array_append(parents, parent_node);
    }

    JSON_DECREF(jn_options);
    JSON_DECREF(refs);
    JSON_DECREF(cols);
    return parents;
}

/***************************************************************************
 *  Return number of parent nodes pointed by the link (fkey)
 ***************************************************************************/
PUBLIC size_t treedb_parents_size(
    json_t *tranger,
    const char *link, // must be a fkey field
    json_t *node // not owned
)
{
    json_t *list = treedb_list_parents(tranger, link, node, 0);
    size_t size = json_array_size(list);
    JSON_DECREF(list);
    return size;
}

/***************************************************************************
 *  Return a list of child nodes of the hook (WARNING ONLY first level)
 ***************************************************************************/
PUBLIC json_t *treedb_list_childs(
    json_t *tranger,
    const char *hook,
    json_t *node, // not owned
    json_t *jn_options // owned, "collapsed"
)
{
    BOOL collapsed = kw_get_bool(jn_options, "collapsed", 0, KW_WILD_NUMBER);
    const char *topic_name = kw_get_str(node, "__md_treedb__`topic_name", 0, KW_REQUIRED);
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);
    BOOL original_node = kw_get_bool(node, "__md_treedb__`__original_node__", 0, 0);

    json_t *col = kw_get_dict_value(cols, hook, 0, 0);
    if(!col) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook not found in the desc",
            "topic_name",   "%s", topic_name,
            "hook",         "%s", hook,
            NULL
        );
        log_debug_json(0, node, "hook not found in the desc");
        JSON_DECREF(jn_options);
        JSON_DECREF(cols);
        return 0;
    }
    json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
    BOOL is_hook = kw_has_word(desc_flag, "hook", 0)?TRUE:FALSE;
    if(!is_hook) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field is not a hook",
            "topic_name",   "%s", topic_name,
            "hook",         "%s", hook,
            NULL
        );
        log_debug_json(0, node, "field is not a hook");
        JSON_DECREF(jn_options);
        JSON_DECREF(cols);
        return 0;
    }
    JSON_DECREF(jn_options);
    JSON_DECREF(cols);

    if(!collapsed) {
        return kwid_new_list("verbose", node, hook);
    }

    json_t *field_value = kw_get_dict_value(node, hook, 0, KW_REQUIRED);
    return get_hook_refs(field_value, original_node);
}

/***************************************************************************
 *  Return number of child nodes of the hook (WARNING ONLY first level)
 ***************************************************************************/
PUBLIC size_t treedb_childs_size(
    json_t *tranger,
    const char *hook,
    json_t *node // not owned
)
{
    json_t *list = treedb_list_childs(tranger, hook, node, 0);
    size_t size = json_array_size(list);
    JSON_DECREF(list);
    return size;
}




                /*----------------------------*
                 *          Schema
                 *----------------------------*/




/***************************************************************************
 *  Return a list with the cols that are links (fkeys) of the topic
 ***************************************************************************/
PUBLIC json_t *treedb_get_topic_links(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    json_t *topics = treedb_topics(tranger, treedb_name, "");
    if(!json_str_in_list(topics, topic_name, 0)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "topic not found",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(topics);
        return 0;
    }
    JSON_DECREF(topics);

    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);
    json_t *jn_list = json_array();

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(desc_flag, "fkey", 0)?TRUE:FALSE;
        if(is_fkey) {
            json_array_append_new(jn_list, json_string(col_name));
        }
    }
    JSON_DECREF(cols);
    return jn_list;
}

/***************************************************************************
 *  Return a list with the cols that are hooks of the topic
 ***************************************************************************/
PUBLIC json_t *treedb_get_topic_hooks(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    json_t *topics = treedb_topics(tranger, treedb_name, "");
    if(!json_str_in_list(topics, topic_name, 0)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "topic not found",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(topics);
        return 0;
    }
    JSON_DECREF(topics);

    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);
    json_t *jn_list = json_array();

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(col, "flag", 0, 0);
        BOOL is_hook = kw_has_word(desc_flag, "hook", 0)?TRUE:FALSE;
        if(is_hook) {
            json_array_append_new(jn_list, json_string(col_name));
        }
    }
    JSON_DECREF(cols);
    return jn_list;
}




                        /*----------------------------*
                         *      Transactions
                         *----------------------------*/




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




                        /*----------------------------*
                         *          Snaps
                         *----------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t * treedb_get_activated_snap_tag(
    json_t *tranger,
    const char *treedb_name,
    uint32_t *user_flag
)
{
    *user_flag = 0;

    json_t *jn_tag = json_pack("{s:b}",
        "active", 1
    );
    json_t *snaps = treedb_list_nodes( // Return MUST be decref
        tranger,
        treedb_name,
        "__snaps__",
        jn_tag,  // filter, owned
        0,  // options, "collapsed"
        0   //match_fn
    );
    int size = json_array_size(snaps);
    if(size == 0) {
        JSON_DECREF(snaps);
        return 0;

    } else if(size > 1) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Too much actives tags",
            NULL
        );
        log_debug_json(0, snaps, "Too much actives tags");

        int idx; json_t *snap;
        json_array_foreach(snaps, idx, snap) {
            if(idx == size -1) {
                break;
            }
            json_object_set_new(snap, "active", json_false());
            treedb_save_node(tranger, snap);
        }

        snap = json_array_get(snaps, size-1);
        *user_flag = (uint32_t)kw_get_int(snap, "id", 0, KW_REQUIRED|KW_WILD_NUMBER);
        JSON_DECREF(snaps);
        return snap;

    } else {
        json_t *snap = json_array_get(snaps, 0);
        *user_flag = kw_get_int(snap, "id", 0, KW_REQUIRED|KW_WILD_NUMBER);
        JSON_DECREF(snaps);
        return snap;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_shoot_snap( // tag the current tree db
    json_t *tranger,
    const char *treedb_name,
    const char *snap_name
)
{
    /*-----------------------------------*
     *  Check if the tag already exists
     *-----------------------------------*/
    json_t *jn_tag = json_pack("{s:s}",
        "name", snap_name
    );
    json_t *snaps = treedb_list_nodes( // Return MUST be decref
        tranger,
        treedb_name,
        "__snaps__",
        jn_tag,  // filter, owned
        0,  // options, "collapsed"
        0   //match_fn
    );
    if(json_array_size(snaps)>0) {
        char temp[80];
        snprintf(temp, sizeof(temp), "Snap already exists: '%s'", snap_name);
        log_info(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", temp,
            "snap",         "%s", snap_name,
            NULL
        );
        JSON_DECREF(snaps);
        return -1;
    }
    JSON_DECREF(snaps);

    /*
     *  Register the tag
     */
    time_t t;
    time(&t);
    struct tm tm;
    gmtime_r(&t, &tm);
    char date[80];
    strftime(date, sizeof(date), "%d %b %Y %T %z", &tm);
    json_t *jn_new_snap = json_pack("{s:s, s:s, s:b}",
        "name", snap_name,
        "date", date,
        "active", 0
    );

    json_t *snap = treedb_create_node( // Return is NOT YOURS
        tranger,
        treedb_name,
        "__snaps__",
        jn_new_snap, // owned
        0
    );

    if(!snap) {
        log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot save record tag",
            "snap",         "%s", snap_name,
            NULL
        );
        return -1;
    }

    uint32_t user_flag = kw_get_int(snap, "id", 0, KW_REQUIRED|KW_WILD_NUMBER);
    if(user_flag==0 || user_flag >= 0xFFFFFFFF) {
        log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "rowid for tag too big",
            "snap",         "%s", snap_name,
            NULL
        );
        return -1;
    }

    int ret = 0;
    json_t *topics = treedb_topics(tranger, treedb_name, "");
    int idx; json_t *jn_topic;
    json_array_foreach(topics, idx, jn_topic) {
        const char *topic_name = json_string_value(jn_topic);
        if(strncmp(topic_name, "__", 2)==0) { // Ignore meta-tables
            continue;
        }

        /*---------------------------------*
         *  Tag each current key of topic
         *---------------------------------*/

        /*----------------------------------*
         *  Get indexx: to tag nodes
         *----------------------------------*/
        json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
        if(!indexx) {
            // It's not a treedb topic
            continue;
        }

        /*
         *  Firstly create a new node. Last node can already have a snap tag
         */
        const char *node_id; json_t *node;
        json_object_foreach(indexx, node_id, node) {
            treedb_save_node(tranger, node);
            uint64_t __rowid__ = kw_get_int(node, "__md_treedb__`__rowid__", 0, KW_REQUIRED);

            ret += tranger_write_user_flag(
                tranger,
                topic_name,
                __rowid__,
                user_flag
            );
            if(ret < 0) {
                log_critical(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Cannot write tag",
                    "topic_name",   "%s", topic_name,
                    "snap",         "%s", snap_name,
                    NULL
                );
            }
        }

        /*
         *  Mark
         */

    }

    JSON_DECREF(topics);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_activate_snap( // Activate tag, return the snap tag
    json_t *tranger,
    const char *treedb_name,
    const char *snap_name
)
{
    if(strcmp(snap_name, "__clear__")==0) {
        /*-------------------------*
         *  Deactivate snap
         *-------------------------*/
        uint32_t user_flag = 0;
        json_t *old_snap = treedb_get_activated_snap_tag(
            tranger,
            treedb_name,
            &user_flag
        );
        if(old_snap) {
            // desactivate tag
            json_object_set_new(old_snap, "active", json_false());
            if(treedb_save_node(tranger, old_snap)<0) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Cannot deactivate snap",
                    "snap",         "%s", snap_name,
                    NULL
                );
            }
        }
        return 0;
    }

    /*--------------------------------*
     *  Recover new activated snap
     *--------------------------------*/
    json_t *jn_tag = json_pack("{s:s}",
        "name", snap_name
    );
    json_t *snaps = treedb_list_nodes( // Return MUST be decref
        tranger,
        treedb_name,
        "__snaps__",
        jn_tag,  // filter, owned
        0,  // options, "collapsed"
        0   //match_fn
    );
    json_t *snap = json_array_get(snaps, 0);
    JSON_DECREF(snaps);

    if(!snap) {
        char temp[80];
        snprintf(temp, sizeof(temp), "Snap not found: '%s'", snap_name);
        log_info(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", temp,
            "snap",         "%s", snap_name,
            NULL
        );
        return -1;
    }

    /*-------------------------------------*
     *  Deactivate current activated snap
     *-------------------------------------*/
    uint32_t user_flag = 0;
    json_t *old_snap = treedb_get_activated_snap_tag(
        tranger,
        treedb_name,
        &user_flag
    );
    if(old_snap) {
        if(snap == old_snap) {
            // Already active
            char temp[80];
            snprintf(temp, sizeof(temp), "Snap already activated: '%s'", snap_name);
            log_info(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", temp,
                "snap",         "%s", snap_name,
                NULL
            );
            return user_flag;
        }
        // desactivate tag
        json_object_set_new(old_snap, "active", json_false());
        if(treedb_save_node(tranger, old_snap)<0) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot deactivate snap",
                "snap",         "%s", kw_get_str(old_snap, "name", "", KW_REQUIRED),
                NULL
            );
        }
    }

    // activate tag
    json_object_set_new(snap, "active", json_true());

    int ret = treedb_save_node(tranger, snap);
    if(ret < 0) {
        return ret;
    }

    return user_flag;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_list_snaps( // Return MUST be decref, list of snaps
    json_t *tranger,
    const char *treedb_name,
    json_t *filter
)
{
    json_t *snaps = treedb_list_nodes( // Return MUST be decref
        tranger,
        treedb_name,
        "__snaps__",
        filter,  // filter, owned
        0,  // options, "collapsed"
        0   //match_fn
    );

    return snaps;
}
