/***********************************************************************
 *          TR_MSG.C
 *
 *          Messages with TimeRanger
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 *
    Load in memory a iter of topic's **active** messages, with n 'instances' of each key.
    If topic tag is 0:
        the active message of each key series is the **last** key instance.
    else:
        the active message is the **last** key instance with tag equal to topic tag.

***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "31_tr_msg.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/


/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
    Open topics for messages (Remember previously open tranger_startup())
 ***************************************************************************/
PUBLIC int trmsg_open_topics(
    json_t *tranger,
    const topic_desc_t *descs
)
{
    for(int i=0; descs[i].topic_name!=0; i++) {
        const topic_desc_t *topic_desc = descs + i;

        tranger_create_topic(
            tranger,    // If topic exists then only needs (tranger,name) parameters
            topic_desc->topic_name,
            topic_desc->pkey,
            topic_desc->tkey,
            topic_desc->system_flag,
            topic_desc->json_desc?create_json_record(topic_desc->json_desc):0 // owned
        );
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int trmsg_add_instance(
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
    trmsg_instance_callback_t trmsg_instance_callback =
        (trmsg_instance_callback_t)(size_t)kw_get_int(
        list,
        "trmsg_instance_callback",
        0,
        0
    );
    if(trmsg_instance_callback) {
        int ret = trmsg_instance_callback(
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
PUBLIC json_t *trmsg_open_list(
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

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int trmsg_close_list(
    json_t *tranger,
    json_t *tr_list
)
{
    return tranger_close_list(tranger, tr_list);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trmsg_get_messages(
    json_t *list
)
{
    return kw_get_dict(list, "messages", 0, KW_REQUIRED);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trmsg_get_message(
    json_t *list,
    const char *key
)
{
    json_t *messages = kw_get_dict(list, "messages", 0, KW_REQUIRED);
    json_t *message = kw_get_dict(messages, key, 0, 0);

    return message;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trmsg_get_active_message(
    json_t *list,
    const char *key
)
{
    json_t *messages = kw_get_dict(list, "messages", 0, KW_REQUIRED);
    json_t *message = kw_get_dict(messages, key, 0, 0);
    if(!message) {
        return 0;
    }
    json_t *active = kw_get_dict_value(message, "active", 0, KW_REQUIRED);
    return active;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trmsg_get_active_md(
    json_t *list,
    const char *key
)
{
    json_t *messages = kw_get_dict(list, "messages", 0, KW_REQUIRED);
    json_t *message = kw_get_dict(messages, key, 0, 0);
    if(!message) {
        return 0;
    }
    json_t *active = kw_get_dict_value(message, "active", 0, KW_REQUIRED);
    json_t *md = kw_get_dict(active, "__md_tranger__", 0, KW_REQUIRED);
    return md;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trmsg_get_instances(
    json_t *list,
    const char *key
)
{
    json_t *messages = kw_get_dict(list, "messages", 0, KW_REQUIRED);
    json_t *message = kw_get_dict(messages, key, 0, 0);
    if(!message) {
        return 0;
    }
    json_t *instances= kw_get_dict_value(message, "instances", 0, KW_REQUIRED);
    return instances;
}

/***************************************************************************
 *  Return a list of records (list of dicts).
 *  WARNING Returned value is yours, must be decref.
 ***************************************************************************/
PUBLIC json_t *trmsg_active_records(
    json_t *list,
    BOOL with_metadata
)
{
    json_t *jn_records = json_array();
    json_t *messages = trmsg_get_messages(list);

    const char *key;
    json_t *message;
    void *n;
    json_object_foreach_safe(messages, n, key, message) {
        json_t *active = json_object_get(message, "active");
        json_t *jn_active = json_deep_copy(active);
        if(with_metadata) {
            json_object_set_new(
                jn_active,
                "__md_tranger__",
                json_deep_copy(
                    json_object_get(
                        active,
                        "__md_tranger__"
                    )
                )
            );
        }
        json_array_append_new(jn_records, jn_active);
    }

    return jn_records;
}

/***************************************************************************
 *  Return a list of record's instances (list of dicts).
 *  WARNING Returned value is yours, must be decref.
 ***************************************************************************/
PUBLIC json_t *trmsg_record_instances(
    json_t *list,
    const char *key,
    BOOL with_metadata
)
{
    json_t *jn_records = json_array();

    json_t *instances = trmsg_get_instances(list, key);

    int idx;
    json_t *jn_value;
    json_array_foreach(instances, idx, jn_value) {
        json_t *jn_record = json_deep_copy(jn_value); // Your copy
        if(with_metadata) {
            json_object_set(
                jn_record,
                "__md_tranger__",
                json_object_get(jn_value, "__md_tranger__")
            );
        }
        json_array_append_new(jn_records, jn_record);
    }

    return jn_records;
}

/***************************************************************************
 *  Foreach active records
 ***************************************************************************/
PUBLIC int trmsg_foreach_active_records(
    json_t *list,
    BOOL with_metadata,
    int (*callback)( // Return < 0 break the foreach
        json_t *list,  // Not yours!
        const char *key,
        json_t *record, // It's yours, Must be owned
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2
)
{
    json_t *messages = trmsg_get_messages(list);

    const char *key;
    json_t *message;
    void *n;
    json_object_foreach_safe(messages, n, key, message) {
        json_t *active = json_object_get(message, "active");
        json_t *jn_active = json_deep_copy(active); // Your copy
        if(with_metadata) {
            json_object_set_new(
                jn_active,
                "__md_tranger__",
                json_deep_copy(
                    json_object_get(
                        active,
                        "__md_tranger__"
                    )
                )
            );
        }

        if(callback(list, key, jn_active, user_data1, user_data2)<0) {
            return -1;
        }
    }

    return 0;
}

/***************************************************************************
 *  Foreach instances records
 ***************************************************************************/
PUBLIC int trmsg_foreach_instances_records(
    json_t *list,
    BOOL with_metadata,
    int (*callback)( // Return < 0 break the foreach
        json_t *list,  // Not yours!
        const char *key,
        json_t *instances, // It's yours, Must be owned
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2
)
{
    json_t *messages = trmsg_get_messages(list);

    const char *key;
    json_t *message;
    void *n;
    json_object_foreach_safe(messages, n, key, message) {
        json_t *instances = json_object_get(message, "instances");
        json_t *jn_instances = json_array(); // Your copy
        int idx;
        json_t *instance;
        json_array_foreach(instances, idx, instance) {
            json_t *jn_instance = json_deep_copy(instance);
            if(with_metadata) {
                json_object_set_new(
                    jn_instance,
                    "__md_tranger__",
                    json_deep_copy(
                        json_object_get(
                            instance,
                            "__md_tranger__"
                        )
                    )
                );
            }
            json_array_append_new(jn_instances, jn_instance);
        }
        if(callback(list, key, jn_instances, user_data1, user_data2)<0) {
            return -1;
        }
    }

    return 0;
}
