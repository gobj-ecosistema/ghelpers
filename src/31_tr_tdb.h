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

    Rules:
        - In links only fkeys are saved in tranger (child node info).
        - Keys (id's) can't contain ` nor ^ characters.
        - A fkey field only can be fkey once, i.e, only can have one parent

**rst**/

/**rst**
    Open tree db (Remember previously open tranger_startup())

    Tree of nodes of data. Node's data contains attributes, in json.

    The nodes are linked with relation parent->child
    in pure-child or multi-parent mode.
    The tree in json too.

    HACK Conventions:
        1) the pkey of all topics must be "id".
        2) the "id" field (primary key) MUST be a string.

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
 *      Utils
 *------------------------------------*/
PUBLIC json_t *_treedb_create_topic_cols_desc(void);
PUBLIC int parse_schema_cols( // Return 0 if ok or # of errors in negative
    json_t *cols_desc,  // not owned
    json_t *data        // owned
);
PUBLIC char *build_treedb_index_path(
    char *bf,
    int bfsize,
    const char *treedb_name,
    const char *topic_name,
    const char *key
);
PUBLIC int parse_hooks(
    json_t *tranger
);

/*------------------------------------*
 *      Manage the tree's nodes
 *------------------------------------*/
PUBLIC json_t *treedb_create_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw, // owned
    const char *options // "permissive" "verbose"
);

PUBLIC int treedb_update_node(
    json_t *tranger,
    json_t *node    // not owned
);

PUBLIC int treedb_delete_node(
    json_t *tranger,
    json_t *node    // owned
);

/***************************************************************************
 * Return a list of hook field names of the topic.
 * Return MUST be decref
 ***************************************************************************/
PUBLIC json_t *tranger_hook_names(
    json_t *topic_desc // owned
);

PUBLIC json_t *tranger_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
);

/**rst**
    Return a view of node with hook fields being collapsed
**rst**/
PUBLIC json_t *tranger_collapsed_view( // Return MUST be decref
    json_t *jn_hook_names, // not owned
    json_t *node // not owned
);

PUBLIC json_t *treedb_list_nodes( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_ids,     // owned
    json_t *jn_filter,  // owned
    json_t *jn_options, // owned "expand"
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);

PUBLIC json_t *treedb_get_index( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *index_name
);

PUBLIC json_t *treedb_get_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *id
);

PUBLIC int treedb_link_nodes(
    json_t *tranger,
    const char *hook,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
);
PUBLIC int treedb_unlink_nodes(
    json_t *tranger,
    const char *hook,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
);

PUBLIC int treedb_link_multiple_nodes(
    json_t *tranger,
    const char *hook,
    json_t *parent_nodes,   // not owned
    json_t *child_nodes     // not owned
);

#ifdef __cplusplus
}
#endif
