/****************************************************************************
 *          TR_TDB.C
 *
 *          Tree Database over TimeRanger
 *
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
    Open tree db (Remember previously open tranger_startup())

    Tree of nodes of data. Node's data contains attributes, in json.

    The nodes are linked with relation parent->child
    in pure-child or multi-parent mode.
    The tree in json too.

**rst**/
PUBLIC json_t *trtdb_open_db( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_schema,  // owned
    json_t *jn_options  // owned
);
PUBLIC int trtdb_close_db(
    json_t *tranger,
    const char *treedb_name
);

PUBLIC int trtdb_tag( // tag the current tree db
    json_t *tranger,
    const char *treedb_name,
    const char *tag
);

PUBLIC int trtdb_reopen_db(
    json_t *tranger,
    const char *treedb_name,
    const char *tag  // If empty tag then free the tree, active record will be the last record.
);

/*------------------------------------*
 *      Testing
 *------------------------------------*/
PUBLIC json_t *_trtdb_create_topic_cols_desc(void);
PUBLIC int parse_schema_cols( // Return 0 if ok or # of errors in negative
    const char *topic_name,
    json_t *cols_desc,
    json_t *data
);

/*------------------------------------*
 *      Manage the tree's nodes
 *------------------------------------*/
PUBLIC json_t *trtdb_read_node( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id,     // owned, Explicit id. Can be: integer,string, [integer], [string], [keys]
    json_t *fields, // owned, Return only this fields. Can be: string, [string], [keys]
    json_t *kw,     // owned, Being filter on reading or record on writting
    const char *options // "create", "delete", "verbose", "metadata"
);

PUBLIC int trtdb_write_node(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id, // owned, Explicit id. Can be: integer,string
    json_t *kw, // owned
    const char *options // "inmediate"
);

PUBLIC json_t *trtdb_select_nodes(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *filter,
    const char *options // "delete", "recursive", "verbose", "metadata"
);

PUBLIC json_t *trtdb_link_node(
    json_t *tranger,
    json_t *kw_parent,
    json_t *kw_child,
    const char *options  // "return-child" (default), "return-parent"
);
PUBLIC json_t *trtdb_unlink_node(
    json_t *tranger,
    json_t *kw_parent,
    json_t *kw_child,
    const char *options  // "return-parent" (default), "return-child"
);

/*------------------------------------*
 *      Transactions
 *------------------------------------*/
PUBLIC int trtdb_begin_transaction(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *options
);

PUBLIC int trtdb_end_transaction(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *options
);


#ifdef __cplusplus
}
#endif
