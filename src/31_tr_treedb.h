/****************************************************************************
 *          TR_TREEDB.C
 *
 *          Tree Database over TimeRanger
 *          Hierarchical tree of objects (nodes, records, messages)
 *          linked by child-parent relation.
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
        - A fkey field only can be fkey once, i.e, only can have one parent (???)

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

    Option "persistent"
        Try to load the schema from file
        File has precedence.
        Once saved,
            if you want to change the schema
            then you must change the schema version and topic_version
**rst**/
PUBLIC json_t *treedb_open_db( // Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
);
PUBLIC int treedb_close_db(
    json_t *tranger,
    const char *treedb_name
);

// Return is NOT YOURS, pkey MUST be "id"
// WARNING This function don't load hook links.
// Intended to use for resources like rc_sqlite3
//   HACK IDEMPOTENT function
PUBLIC json_t *treedb_create_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *topic_version,
    json_t *jn_cols // owned
);
PUBLIC int treedb_delete_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

PUBLIC json_t *treedb_list_treedb( // Return a list with treedb names
    json_t *tranger
);

PUBLIC json_t *treedb_list_topics(
    json_t *tranger,
    const char *treedb_name,
    const char *options // "dict" return list of dicts, otherwise return list of strings
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
PUBLIC json_t *treedb_get_index( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *index_name
);


/**rst**
    Return refs of fkeys of col_name field
**rst**/
PUBLIC json_t *treedb_node_up_refs(  // Return MUST be decref
    json_t *tranger,
    json_t *node,    // not owned
    const char *topic_name,
    const char *col_name
);

/**rst**
    Return array of `parent_id` of `value`  (refs: parent_topic_name^parent_id^hook_name)
**rst**/
PUBLIC json_t *treedb_beatiful_up_refs(  // Return MUST be decref
    json_t *value
);


/*------------------------------------*
 *      Manage the tree's nodes
 *------------------------------------*/

/**rst**
    WARNING This function does NOT auto build links
**rst**/
PUBLIC json_t *treedb_create_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw, // owned
    const char *options // "permissive"
);

PUBLIC int treedb_save_node(
    json_t *tranger,
    json_t *node    // not owned
);

/**rst**
    WARNING This function DOES auto build links

    "create" ["permissive"] create node if not exist
    "clean" clean wrong fkeys
**rst**/
PUBLIC json_t *treedb_update_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,    // owned
    const char *options // "create" ["permissive"], "clean"
);

/**rst**
    "force" delete links. If there are links are not force then delete_node will fail
**rst**/
PUBLIC int treedb_delete_node(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *node,    // owned
    const char *options // "force"
);

PUBLIC int treedb_link_nodes(
    json_t *tranger,
    const char *hook,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
);

PUBLIC int treedb_link_nodes2(
    json_t *tranger,
    const char *treedb_name,
    const char *parent_ref,     // parent_topic_name^parent_id^hook_name
    const char *child_ref       // child_topic_name^child_id
);

PUBLIC int treedb_unlink_nodes(
    json_t *tranger,
    const char *hook,
    json_t *parent_node,    // not owned
    json_t *child_node      // not owned
);

PUBLIC int treedb_unlink_nodes2(
    json_t *tranger,
    const char *treedb_name,
    const char *parent_ref,     // parent_topic_name^parent_id^hook_name
    const char *child_ref       // child_topic_name^child_id
);

PUBLIC int treedb_link_multiple_nodes(
    json_t *tranger,
    const char *hook,
    json_t *parent_nodes,   // not owned
    json_t *child_nodes     // not owned
);

/**rst**
    Return a list of matched nodes
    If collapsed:
        - the ref (fkeys to up) have 3 ^ fields
        - the ref (childs, to down) have 2 ^ fields
    WARNING Returned list in "collapsed" mode have duplicated nodes,
            otherwise it returns original nodes.
**rst**/
PUBLIC json_t *treedb_list_nodes( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_ids,     // owned
    json_t *jn_filter,  // owned
    json_t *jn_options, // owned "collapsed"
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);

PUBLIC json_t *treedb_get_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *id
);

/*
 *  Devuelve una vista del node collapsed
 */
PUBLIC json_t *treedb_collapse_node( // Return MUST be decref
    json_t *tranger,
    json_t *node // not owned
);

/*
 *  Return a list of parent nodes pointed by the link
 */
PUBLIC json_t *treedb_get_parent_nodes( // Return MUST be decref
    json_t *tranger,
    json_t *node, // not owned
    const char *link // must be a fkey field
);


#ifdef __cplusplus
}
#endif
