/****************************************************************************
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

 ****************************************************************************/
#pragma once

#include "30_timeranger.h"
#include "31_tr_treedb.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Desc
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
/**rst**

    Rules:
        - Keys (id's) can't contain ` nor ^ characters.
        - A fkey field only can be fkey once, i.e, only can have one parent

**rst**/

/**rst**
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
**rst**/

PUBLIC int trmsg2_open_db(
    json_t *tranger,
    const char *msg2db_name,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
);

PUBLIC int trmsg2_close_db(
    json_t *tranger,
    const char *msg2db_name
);


PUBLIC json_t *trmsg2_add_instance( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *kw, // owned
    const char *options // "permissive"
);


/*
    jn_filter (match_cond) of second level:

        max_key_instances   (int) Maximum number of instances per key.

        order_by_tm         (bool) Not use with max_key_instances=1

        trmsg2_instance_callback (int) callback function,
                                     inform of loaded instance, chance to change content

        select_fields       (dict or list of strings) Display ony selected fields
        match_fields        (dict or list of dicts) Load instances that matches fields

 */

PUBLIC json_t *trmsg2_list_messages( // Return MUST be decref
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);

PUBLIC json_t *msg2db_get_message( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id,
    const char *id2
);
PUBLIC json_t *msg2db_get_message_active( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id,
    const char *id2
);
PUBLIC json_t *msg2db_get_message_instances( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id,
    const char *id2
);

#ifdef __cplusplus
}
#endif
