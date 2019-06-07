/****************************************************************************
 *          TR_QUEUE.H
 *
 *          Persistent queue based in TimeRanger
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/

#pragma once

#include <time.h>
#include "30_timeranger.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define __MD_TRQ__ "__md_trq__"

#define TRQ_MSG_PENDING   0x0001

/***************************************************************
 *              Structures
 ***************************************************************/
typedef void *tr_queue;
typedef void *q_msg;

/***************************************************************
 *              Prototypes
 ***************************************************************/

/**rst**
    Open queue (Remember previously open tranger_startup())
**rst**/
PUBLIC tr_queue trq_open(
    json_t *tranger,
    const char *topic_name,
    const char *pkey,
    const char *tkey,
    system_flag_t system_flag,
    size_t backup_queue_size
);

/**rst**
    Close queue (After close the queue, remember do tranger_shutdown())
**rst**/
PUBLIC void trq_close(tr_queue trq);

/**rst**
    Return size of queue (messages in queue)
**rst**/
PUBLIC size_t trq_size(tr_queue trq);

/**rst**
    Return tranger of queue
**rst**/
PUBLIC json_t * trq_tranger(tr_queue trq);

/**rst**
    Return topic of queue
**rst**/
PUBLIC json_t * trq_topic(tr_queue trq);

/**rst**
    Set from_rowid to improve speed
**rst**/
PUBLIC void trq_set_first_rowid(tr_queue trq, uint64_t first_rowid);

/**rst**
    Load pending messages, return a iter
**rst**/
PUBLIC int trq_load(tr_queue trq);

/**rst**
    Load all messages, return a iter
**rst**/
PUBLIC int trq_load_all(tr_queue trq, const char *key, int64_t from_rowid, int64_t to_rowid);

/**rst**
    Append a new message to queue
**rst**/
PUBLIC q_msg trq_append(
    tr_queue trq,
    json_t *jn_msg  // owned
);

/**rst**
    Get a message from iter by his key
**rst**/
PUBLIC q_msg trq_get_by_rowid(tr_queue trq, uint64_t rowid);

/**rst**
    Check pending status of a rowid (low level)
    Return -1 if rowid not exists, 1 if pending, 0 if not pending
**rst**/
PUBLIC int trq_check_pending_rowid(tr_queue trq, uint64_t rowid);

/**rst**
    Unload a message from iter
**rst**/
PUBLIC void trq_unload_msg(q_msg msg, int32_t result);

/**rst**
    Mark a message.
    You must flag a message after append to queue
        if you want recover it in the next open
        with the flag used in trq_load()
**rst**/
PUBLIC int trq_set_hard_flag(q_msg msg, uint32_t hard_mark, BOOL set);

/**rst**
    Set soft mark
**rst**/
PUBLIC uint64_t trq_set_soft_mark(q_msg msg, uint64_t soft_mark, BOOL set);

/**rst**
    Get if it's msg pending of ack
**rst**/
PUBLIC uint64_t trq_get_soft_mark(q_msg msg);

/**rst**
    Set ack timer
**rst**/
PUBLIC time_t trq_set_ack_timer(q_msg msg, time_t seconds);

/**rst**
    Clear ack timer
**rst**/
PUBLIC void trq_clear_ack_timer(q_msg msg);

/**rst**
    Test ack timer
**rst**/
PUBLIC BOOL trq_test_ack_timer(q_msg msg);

/**rst**
    Walk over instances
**rst**/
#define qmsg_foreach_forward(trq, msg) \
    for(msg = trq_first_msg(trq); \
        msg!=0 ; \
        msg = trq_next_msg(msg))

#define qmsg_foreach_backward(trq, msg) \
    for(msg = trq_last_msg(trq); \
        msg!=0 ; \
        msg = trq_prev_msg(msg))

/*
 *  Example

    q_msg msg;
    qmsg_foreach_forward(trq, msg) {
        json_t *jn_gps_msg = trq_msg_json(msg);
    }

*/
PUBLIC q_msg trq_first_msg(tr_queue trq);
PUBLIC q_msg trq_last_msg(tr_queue trq);
PUBLIC q_msg trq_next_msg(q_msg msg);
PUBLIC q_msg trq_prev_msg(q_msg msg);

/**rst**
    Get info of message
**rst**/
PUBLIC md_record_t trq_msg_md_record(q_msg msg);
PUBLIC uint64_t trq_msg_rowid(q_msg msg);
PUBLIC json_t *trq_msg_json(q_msg msg); // Return json is NOT YOURS!!
PUBLIC uint64_t trq_msg_time(q_msg msg);

/**rst**
    Metadata
**rst**/
PUBLIC int trq_set_metadata(
    json_t *kw,
    const char *key,
    json_t *jn_value // owned
);
PUBLIC json_t *trq_get_metadata( // WARNING The return json is not yours!
    json_t *kw
);

/**rst**
    Return a new kw only with the message metadata
**rst**/
PUBLIC json_t *trq_answer(
    json_t *jn_message,  // not owned, Gps message, to get only __MD_TRQ__
    int result
);

/**rst**
    Do backup if needed.
**rst**/
PUBLIC int trq_check_backup(tr_queue trq);

#ifdef __cplusplus
}
#endif

