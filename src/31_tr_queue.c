/***********************************************************************
 *          TR_QUEUE.C
 *
 *          Persistent queue based in TimeRanger
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include "31_tr_queue.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct {
    json_t *tranger;
    json_t *topic;
    char topic_name[128];
    int maximum_retries;
    dl_list_t dl_q_msg;
} tr_queue_t;

typedef struct {
    DL_ITEM_FIELDS

    tr_queue_t *trq;
    md_record_t md_record;
    uint64_t mark;          // soft mark.
    time_t timeout_ack;
    int retries;
    json_t *jn_record;
} q_msg_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void free_msg(void *msg_);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
    Open queue
 ***************************************************************************/
PUBLIC tr_queue trq_open(
    json_t *tranger,
    const char *topic_name,
    const char *pkey,
    const char *tkey,
    system_flag_t system_flag,
    size_t backup_queue_size
)
{
    /*-------------------------------*
     *  Alloc memory
     *-------------------------------*/
    tr_queue_t *trq = gbmem_malloc(sizeof(tr_queue_t));
    if(!trq) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot create tr_queue. gbmem_malloc() FAILED",
            NULL
        );
        return 0;
    }
    trq->tranger = tranger;
    snprintf(trq->topic_name, sizeof(trq->topic_name), "%s", topic_name);

    /*-------------------------------*
     *  Open/Create topic
     *-------------------------------*/
    trq->topic = tranger_create_topic( // IDEMPOTENT function
        trq->tranger,
        topic_name,
        pkey,
        tkey,
        system_flag,
        0,
        0
    );
    if(!trq->topic) {
        trq_close(trq);
        return 0;
    }
    dl_init(&trq->dl_q_msg);

    if(backup_queue_size > 0 && kw_get_bool(trq->tranger, "master", 0, KW_REQUIRED)) {
        json_t *jn_topic_var = json_object();
        json_object_set_new(jn_topic_var, "backup_queue_size", json_integer(backup_queue_size));
        tranger_write_topic_var(
            trq->tranger,
            topic_name,
            jn_topic_var  // owned
        );
    }

    return trq;
}

/***************************************************************************
    Close queue (After close the queue remember tranger_shutdown())
 ***************************************************************************/
PUBLIC void trq_close(tr_queue trq)
{
    dl_flush(&((tr_queue_t *)trq)->dl_q_msg, free_msg);
    gbmem_free(trq);
}

/***************************************************************************
    Return size of queue (messages in queue)
 ***************************************************************************/
PUBLIC size_t trq_size(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return dl_size(&((tr_queue_t *)trq)->dl_q_msg);
}

/***************************************************************************
    Return tranger of queue
 ***************************************************************************/
PUBLIC json_t * trq_tranger(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return ((tr_queue_t *)trq)->tranger;
}

/***************************************************************************
    Return topic of queue
 ***************************************************************************/
PUBLIC json_t * trq_topic(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return ((tr_queue_t *)trq)->topic;
}

/***************************************************************************
    Set first rowid to search
 ***************************************************************************/
PUBLIC void trq_set_first_rowid(tr_queue trq_, uint64_t first_rowid)
{
    register tr_queue_t *trq = trq_;

    if(kw_get_bool(trq->tranger, "master", 0, KW_REQUIRED)) {
        json_t *jn_topic_var = json_object();
        json_object_set_new(jn_topic_var, "first_rowid", json_integer(first_rowid));
        tranger_write_topic_var(
            trq->tranger,
            tranger_topic_name(trq->topic),
            jn_topic_var  // owned
        );
    }
}

/***************************************************************************
    New msg
 ***************************************************************************/
PRIVATE q_msg_t *new_msg(
    tr_queue_t *trq,
    const md_record_t *md_record,
    json_t *jn_record // owned
)
{
    if(!jn_record) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Json record is EMPTY",
            NULL
        );
        return 0;
    }
    /*
     *  Alloc memory
     */
    q_msg_t *msg = gbmem_malloc(sizeof(q_msg_t));
    if(!msg) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot create msg. gbmem_malloc() FAILED",
            NULL
        );
        return 0;
    }
    memmove(&msg->md_record, md_record, sizeof(md_record_t));
    msg->jn_record = 0; // Cárgalo solo cuando se use, jn_record;
    JSON_DECREF(jn_record);
    msg->trq = trq;
    dl_add(&trq->dl_q_msg, msg);

    return msg;
}

/***************************************************************************
    Free msg
 ***************************************************************************/
PRIVATE void free_msg(void *msg_)
{
    q_msg_t *msg = msg_;
    JSON_DECREF(msg->jn_record);
    memset(msg, 0, sizeof(q_msg_t));
    gbmem_free(msg);
}

/***************************************************************************

 ***************************************************************************/
static uint64_t first_rowid = 0;  // usado temporalmente

PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
)
{
    tr_queue_t *trq = (tr_queue_t *)(size_t)kw_get_int(list, "trq", 0, KW_REQUIRED);

    if(first_rowid==0) {
        first_rowid = md_record->__rowid__;
    }

    new_msg(trq, md_record, jn_record);

    return 0;
}

PUBLIC int trq_load(tr_queue trq_)
{
    register tr_queue_t *trq = trq_;

    json_t *match_cond = json_object();
    json_object_set_new(
        match_cond,
        "user_flag_mask_set",
        json_integer(TRQ_MSG_PENDING)
    );

    uint64_t last_first_rowid = kw_get_int(trq->topic, "first_rowid", 0, 0);
    if(last_first_rowid) {
        if(last_first_rowid <= tranger_topic_size(trq->topic)) {
            json_object_set_new(match_cond, "from_rowid", json_integer(last_first_rowid));
        }
    }

    first_rowid = 0;

    json_t *jn_list = json_pack("{s:s, s:o, s:I, s:I}",
        "topic_name", trq->topic_name,
        "match_cond", match_cond,
        "load_record_callback", (json_int_t)(size_t)load_record_callback,
        "trq", (json_int_t)(size_t)trq
    );
    json_t *tr_list = tranger_open_list(
        trq->tranger,
        jn_list
    );
    tranger_close_list(trq->tranger, tr_list);

    if(first_rowid==0) {
        // No hay ningún msg pending, pon la última rowid
        first_rowid = tranger_topic_size(trq->topic);
    }
    if(first_rowid) {
        trq_set_first_rowid(trq, first_rowid);
    }

    return 0;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int trq_load_all(tr_queue trq_, const char *key, int64_t from_rowid, int64_t to_rowid)
{
    register tr_queue_t *trq = trq_;

    json_t *match_cond = json_object();
    if(from_rowid) {
        json_object_set_new(match_cond, "from_rowid", json_integer(from_rowid));
    }
    if(to_rowid) {
        json_object_set_new(match_cond, "to_rowid", json_integer(to_rowid));
    }
    if(key) {
        json_object_set_new(
            match_cond,
            "key",
            json_string(key)
        );
    }

    json_t *jn_list = json_pack("{s:s, s:o, s:I, s:I}",
        "topic_name", tranger_topic_name(trq->topic),
        "match_cond", match_cond,
        "load_record_callback", (json_int_t)(size_t)load_record_callback,
        "trq", (json_int_t)(size_t)trq
    );
    json_t *tr_list = tranger_open_list(
        trq->tranger,
        jn_list
    );
    tranger_close_list(trq->tranger, tr_list);

    return 0;
}

/***************************************************************************
    Append a pending message to queue
 ***************************************************************************/
PUBLIC q_msg trq_append(
    tr_queue trq_,
    json_t *jn_msg  // owned
)
{
    register tr_queue_t *trq = trq_;

    if(!jn_msg || jn_msg->refcount <= 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "jn_msg NULL",
            "topic",        "%s", tranger_topic_name(trq->topic),
            NULL
        );
        return 0;
    }

    JSON_INCREF(jn_msg);
    md_record_t md_record;
    tranger_append_record(
        trq->tranger,
        tranger_topic_name(trq->topic),
        0,      // __t__
        TRQ_MSG_PENDING,    // __flag__
        &md_record,
        jn_msg // owned
    );
    q_msg_t *msg = new_msg(
        trq,
        &md_record,
        jn_msg  // owned
    );
    return msg;
}

/***************************************************************************
    Get a message from iter by his rowid
 ***************************************************************************/
PUBLIC q_msg trq_get_by_rowid(tr_queue trq, uint64_t rowid)
{
    register q_msg_t *msg;

    qmsg_foreach_forward(trq, msg) {
        if(msg->md_record.__rowid__ == rowid) {
            return msg;
        }
    }

    return 0;
}

/***************************************************************************
    Get a message from iter by his key
 ***************************************************************************/
PUBLIC q_msg trq_get_by_key(tr_queue trq, const char *key)
{
    register q_msg_t *msg;

    qmsg_foreach_forward(trq, msg) {
        if(strcmp(msg->md_record.key.s, key)==0) {
            return msg;
        }
    }

    return 0;
}

/***************************************************************************
    Get number of messages from iter by his key
 ***************************************************************************/
PUBLIC int trq_size_by_key(tr_queue trq, const char *key)
{
    register q_msg_t *msg;
    int n = 0;
    qmsg_foreach_forward(trq, msg) {
        if(strcmp(msg->md_record.key.s, key)==0) {
            n++;
        }
    }

    return n;
}

/***************************************************************************
    Check pending status of a rowid (low level)
 ***************************************************************************/
PUBLIC int trq_check_pending_rowid(tr_queue trq_, uint64_t rowid)
{
    register tr_queue_t *trq = trq_;

    uint32_t __user_flag__ = tranger_read_user_flag(
        trq->tranger,
        tranger_topic_name(trq->topic),
        rowid
    );
    if(__user_flag__ & TRQ_MSG_PENDING) {
        return 1;
    } else {
        return 0;
    }
}

/***************************************************************************
    Unload a message from iter
 ***************************************************************************/
PUBLIC void trq_unload_msg(q_msg msg_, int32_t result)
{
    q_msg_t *msg = msg_;

    // TODO guarda result
    trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0);

    dl_delete(&msg->trq->dl_q_msg, msg, free_msg);
}

/***************************************************************************
    Put a hard flag in a message.
    You must flag a message after append it if you want recover it in the next open.
 ***************************************************************************/
PUBLIC int trq_set_hard_flag(q_msg msg_, uint32_t hard_mark, BOOL set)
{
    register q_msg_t *msg = msg_;

    return tranger_set_user_flag(
        msg->trq->tranger,
        tranger_topic_name(msg->trq->topic),
        msg->md_record.__rowid__,
        hard_mark,
        set
    );
}

/***************************************************************************
    Set soft mark
 ***************************************************************************/
PUBLIC uint64_t trq_set_soft_mark(q_msg msg, uint64_t soft_mark, BOOL set)
{
    if(set) {
        /*
         *  Set
         */
        ((q_msg_t *)msg)->mark |= soft_mark;
    } else {
        /*
         *  Reset
         */
        ((q_msg_t *)msg)->mark &= ~soft_mark;
    }

    return ((q_msg_t *)msg)->mark;
}

/***************************************************************************
    Get soft mark
 ***************************************************************************/
PUBLIC uint64_t trq_get_soft_mark(q_msg msg)
{
    return ((q_msg_t *)msg)->mark;
}

/***************************************************************************
    Set ack timer
 ***************************************************************************/
PUBLIC time_t trq_set_ack_timer(q_msg msg, time_t seconds)
{
    ((q_msg_t *)msg)->timeout_ack = start_sectimer(seconds);
    return ((q_msg_t *)msg)->timeout_ack;
}

/***************************************************************************
    Clear ack timer
 ***************************************************************************/
PUBLIC void trq_clear_ack_timer(q_msg msg)
{
    ((q_msg_t *)msg)->timeout_ack = -1;
}

/***************************************************************************
    Test ack timer
 ***************************************************************************/
PUBLIC BOOL trq_test_ack_timer(q_msg msg)
{
    return test_sectimer(((q_msg_t *)msg)->timeout_ack);
}

/***************************************************************************
    Set maximum retries
 ***************************************************************************/
PUBLIC void trq_set_maximum_retries(tr_queue trq, int maximum_retries)
{
    ((tr_queue_t *)trq)->maximum_retries = maximum_retries;
}

/***************************************************************************
    Add retries
 ***************************************************************************/
PUBLIC void trq_add_retries(q_msg msg, int retries)
{
    ((q_msg_t *)msg)->retries += retries;
}

/***************************************************************************
    Clear retries
 ***************************************************************************/
PUBLIC void trq_clear_retries(q_msg msg)
{
    ((q_msg_t *)msg)->retries = 0;
}

/***************************************************************************
    Test retries
 ***************************************************************************/
PUBLIC BOOL trq_test_retries(q_msg msg)
{
    if( ((q_msg_t *)msg)->trq->maximum_retries <= 0) {
        // No retries no test true
        return 0;
    }
    return (((q_msg_t *)msg)->retries >= ((q_msg_t *)msg)->trq->maximum_retries)?TRUE:FALSE;
}

/***************************************************************************
    Walk over instances
 ***************************************************************************/
PUBLIC q_msg trq_first_msg(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return dl_first(&((tr_queue_t *)trq)->dl_q_msg);
}
PUBLIC q_msg trq_last_msg(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return dl_last(&((tr_queue_t *)trq)->dl_q_msg);
}

PUBLIC q_msg trq_next_msg(q_msg msg)
{
    return dl_next(msg);
}
PUBLIC q_msg trq_prev_msg(q_msg msg)
{
    return dl_prev(msg);
}

/***************************************************************************
    Get info of message
 ***************************************************************************/
PUBLIC md_record_t trq_msg_md_record(q_msg msg_)
{
    register q_msg_t *msg = msg_;
    return msg->md_record;
}
PUBLIC uint64_t trq_msg_rowid(q_msg msg_)
{
    register q_msg_t *msg = msg_;
    if(!msg) {
        return 0;
    }
    return msg->md_record.__rowid__;
}
PUBLIC json_t *trq_msg_json(q_msg msg_) // Return json is NOT YOURS!!
{
    register q_msg_t *msg = msg_;

    if(!msg->jn_record) {
        msg->jn_record = tranger_read_record_content(
            msg->trq->tranger,
            msg->trq->topic,
            &msg->md_record
        );
    }
    return msg->jn_record;
}
PUBLIC uint64_t trq_msg_time(q_msg msg)
{
    return ((q_msg_t *)msg)->md_record.__t__;
}
PUBLIC BOOL trq_msg_is_t_ms(q_msg msg)
{
    return (((q_msg_t *)msg)->md_record.__system_flag__ & sf_t_ms)?TRUE:FALSE;
}
PUBLIC BOOL trq_msg_is_tm_ms(q_msg msg)
{
    return (((q_msg_t *)msg)->md_record.__system_flag__ & sf_tm_ms)?TRUE:FALSE;
}


/***************************************************************************
    Metadata
 ***************************************************************************/
PUBLIC int trq_set_metadata(
    json_t *kw,
    const char *key,
    json_t *jn_value) // owned
{
    return kw_set_subdict_value(
        kw,
        __MD_TRQ__,
        key,
        jn_value // owned
    );
}

PUBLIC json_t *trq_get_metadata( // WARNING The return json is not yours!
    json_t *kw
)
{
    return kw_get_dict(
        kw,
        __MD_TRQ__,
        0,
        KW_REQUIRED
    );
}

/***************************************************************************
 *  Return a new kw only with the message metadata
 ***************************************************************************/
PUBLIC json_t *trq_answer(
    json_t *jn_message,  // not owned, Gps message, to get only __MD_TRQ__
    int result
)
{
    json_t *kw_response = json_object();
    json_t *__md__ = trq_get_metadata(jn_message);
    if(__md__) {
        json_object_set_new(kw_response, __MD_TRQ__, kw_duplicate(__md__));
        trq_set_metadata(kw_response, "result", json_integer(result));
    }

    return kw_response;
}

/***************************************************************************
 *  Check if backup is needed.
 ***************************************************************************/
PUBLIC int trq_check_backup(tr_queue trq_)
{
    tr_queue_t *trq = trq_;

    uint64_t backup_queue_size = kw_get_int(trq->topic, "backup_queue_size", 0, 0);

    if(backup_queue_size) {
        if(tranger_topic_size(trq->topic) > backup_queue_size) {
            char *topic_name = gbmem_strdup(tranger_topic_name(trq->topic));
            if(topic_name) {
                trq_set_first_rowid(trq, tranger_topic_size(trq->topic)); // WARNING danger change?!
                trq->topic = tranger_backup_topic(
                    trq->tranger,
                    topic_name,
                    0,
                    0,
                    TRUE,
                    0
                );
                trq_set_first_rowid(trq, 0);
                gbmem_free(topic_name);
            }
        }
    }
    return 0;
}
