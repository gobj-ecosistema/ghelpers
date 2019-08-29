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

    HACK Convention: the pkey of all topics must be "id".

**rst**/
PUBLIC json_t *treedb_open_db( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_schema,  // owned
    const char *options
);
PUBLIC int treedb_close_db(
    json_t *tranger,
    const char *treedb_name
);

/*------------------------------------*
 *      Testing
 *------------------------------------*/
PUBLIC json_t *_treedb_create_topic_cols_desc(void);
PUBLIC int parse_schema_cols( // Return 0 if ok or # of errors in negative
    json_t *cols_desc,
    json_t *data
);

/*------------------------------------*
 *      Manage the tree's nodes
 *------------------------------------*/
PUBLIC json_t *treedb_read_node( // Working with explicit 'id' returns a dict, without returns a list
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id,     // owned, Explicit id. Can be: integer,string, [integer], [string], [keys]
    json_t *kw,     // owned, HACK kw is `filter` on read/delete and `record` on create
    const char *options // "create", TODO "delete",
);

PUBLIC int treedb_write_node(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *id, // owned, Explicit id. Can be: integer,string
    json_t *kw, // owned
    const char *options // "strict"
);

PUBLIC int treedb_link_nodes(
    json_t *tranger,
    const char *treedb_name,
    const char *link,
    json_t *parent_records, // not owned
    json_t *child_records,  // not owned
    const char *options     //
);
PUBLIC int treedb_unlink_nodes( // TODO
    json_t *tranger,
    const char *treedb_name,
    const char *link,
    json_t *parent_records, // not owned
    json_t *child_records,  // not owned
    const char *options     //
);


#ifdef __cplusplus
}
#endif
