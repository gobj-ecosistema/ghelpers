/****************************************************************************
 *          TR_TABLE.C
 *
 *          Table (topic) with TimeRanger
 *

 Example of `list`:

 {
    "topic_name": "gps_events",
    "match_cond": {
        "max_key_instances": 1
    },
    "load_record_callback": 94862617778903,
    "record_loaded_callback": 0,
    "messages": {
        ----
    }
}

 Example of `messages`:

{
    "352093088196125": {
        "instances": [
            {
                "__md_tranger__": {
                    "__rowid__": 13607,
                    "__t__": 1555318428,
                    "__tm__": 1555318405000,
                    "__offset__": 3216,
                    "__size__": 402,
                    "__user_flag__": 0,
                    "__system_flag__": 16777729
                },
                "content": {
                    "imei": "352093088196125",
                    "msg": "event",
                    "t_rx": 1555318428,
                    ...
                }
            }
        ],
        "active": {
            "__md_tranger__": {
                "__rowid__": 13607,
                "__t__": 1555318428,
                ...
            },
            "content": {
                "imei": "352093088196125",
                "msg": "event",
                "t_rx": 1555318428,
                ...
            }
        }
    },
    "key2": {
        ...
    },
    ...
}

 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "30_timeranger.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define TRTB_CLOSE_LIST(tranger, tr_list) \
    if(tr_list) {\
        trtb_close_list(tranger, tr_list);\
        tr_list=0;\
    }

/***************************************************************
 *              Structures
 ***************************************************************/
typedef enum {
    fc_only_desc_cols       = 0x00000001,   // save only cols defined in describe
} cols_flag_t;

typedef struct {
    const char *topic_name;
    const char *pkey;   // Primary key
    const system_flag_t system_flag;
    const char *tkey;   // Time key
    const json_desc_t *json_desc;
} topic_desc_t;

/***************************************************************
 *              Desc
 ***************************************************************/

#ifdef __DB_TRANGER_C__     /* ONLY to show json structures */

// TOPIC
static const json_desc_t topic_json_desc[] = {
    // Name             Type        Default
    {"pkey",            "str",      ""},            // Primary key of topic
    {"tkey",            "str",      ""},            // Time key of topic
    {"desc",            "dict",     "{}"},          // Desc of topic
    {"lists",           "list",     "[LIST]"},      // Opened lists of topic
    {0}
};

// LIST
static const json_desc_t list_json_desc[] = {
    // Name             Type        Default
    {"topic_name",      "str",      ""},            // List's topic
    {"match_cond",      "dict",     "{}"},          // Filter of messages
    {"messages",        "dict",     "{MESSAGE}"},   // Messages
    {0}
};

// MESSAGE
static const json_desc_t message_json_desc[] = {
    // Name             Type       Default
    {"active",          "dict",    "{INSTANCE}"},  // Active Instance
    {"instances",       "list",    "[INSTANCE]"},  // Instances of Message
    {0}
};

// INSTANCE
static const json_desc_t instance_json_desc[] = {
    // Name             Type       Default
    {"__md_tranger__",  "dict",    "{}"},    // Instance Metadata
    {"content",         "dict",    "{}"},    // Instance Content
    {0}
};

static topic_desc_t db_tranger_desc[] = {
    // Topic Name,      Pkey        Key Type    Tkey        Topic Json Desc
    {"TOPIC",           "",         0,          "",         topic_json_desc},
    {"LIST",            "",         0,          "",         list_json_desc},
    {"MESSAGE",         "",         0,          "",         message_json_desc},
    {"INSTANCE",        "",         0,          "",         instance_json_desc},
    {0}
};

#endif /* __DB_TRANGER_C__ */

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC json_t *trtb_open_db(
    json_t *jn_tranger,    // owned
    const topic_desc_t *descs
);

PUBLIC void trtb_close_db(json_t *trdb);

PUBLIC int trtb_set_topic_tag( // this change the active record, you must re-open lists of messages
    json_t *tranger,
    const char *topic_name,
    uint32_t topic_tag
);

PUBLIC int trtb_add_instance(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_msg,  // owned
    uint32_t tag,
    cols_flag_t cols_flag,
    md_record_t *md_record
);

/*
    jn_filter (match_cond) of second level:

        max_key_instances   (int) Maximum number of instances per key.

        clone_user_keys     (bool) Clone or share the user keys (math_cond after extracting system keys)
                            Remains of jn_filter are set in json instance
                            (act as shared data or per instance data)

        order_by_tm         (bool) Not use with max_key_instances=1

        trtb_instance_callback (int) callback function,
                                     inform of loaded instance, chance to change content

        select_fields       (dict or list of strings) Display ony selected fields
        match_fields        (dict or list of dicts) Load instances that matches fields

 */

/*
 *  HACK Return of callback:
 *      0 do nothing (callback will create their own list, or not),
 *      1 add record to returned list.data,
 *      -1 break the load
 */
typedef int (*trtb_instance_callback_t)(
    json_t *tranger,    // not yours
    json_t *list,       // not yours
    BOOL is_active,
    json_t *instance    // not yours
);

PUBLIC json_t *trtb_open_list(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_filter  // owned
);

PUBLIC int trtb_close_list(
    json_t *tranger,
    json_t *tr_list
);

PUBLIC json_t *trtb_backup_topic( // TODO not implemented
    json_t *tranger,
    const char *topic_name,
    const char *backup_path,
    const char *backup_name,
    BOOL overwrite_backup,
    tranger_backup_deleting_callback_t tranger_backup_deleting_callback
);


/*
 *  Functions returning items of list (NOT YOURS).
 */
PUBLIC json_t *trtb_get_messages( // Return (NOT yours) dict: messages`
    json_t *list
);
PUBLIC json_t *trtb_get_message( // Return (NOT yours) dict: "active" (dict) and "instances" (list)
    json_t *list,
    const char *key
);
PUBLIC json_t *trtb_get_active_content( // Return (NOT yours) dict: messages`message(key)`active`content
    json_t *list,
    const char *key
);
PUBLIC json_t *trtb_get_active_md( // Return (NOT yours) dict: messages`message(key)`active`md
    json_t *list,
    const char *key
);
PUBLIC json_t *trtb_get_instances( // Return (NOT yours) list: messages`message(key)`instances
    json_t *list,
    const char *key
);


/*
 *  Return a list of records (list of dicts).
 *  WARNING Returned value is yours, must be decref.
 */
PUBLIC json_t *trtb_active_records(
    json_t *list,
    BOOL with_metadata
);

/*
 *  Return a list of record's instances (list of dicts).
 *  WARNING Returned value is yours, must be decref.
 */
PUBLIC json_t *trtb_record_instances(
    json_t *list,
    const char *key,
    BOOL with_metadata
);

/*
 *  Foreach active records
 */
PUBLIC int trtb_foreach_active_records(
    json_t *list,
    BOOL with_metadata,
    int (*callback)( // Return < 0 break the foreach
        void *user_data,
        json_t *list,  // Not yours!
        const char *key,
        json_t *record // It's yours, Must be owned
    ),
    void *user_data
);

/*
 *  Foreach instances records
 */
PUBLIC int trtb_foreach_instances_records(
    json_t *list,
    BOOL with_metadata,
    int (*callback)( // Return < 0 break the foreach
        void *user_data,
        json_t *list,  // Not yours!
        const char *key,
        json_t *instances // It's yours, Must be owned
    ),
    void *user_data
);

#ifdef __cplusplus
}
#endif
