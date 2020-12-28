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

#include "15_webix_helper.h"
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

    Return a dict inside of tranger with path "treedbs`{treedb_name}" DO NOT use it directly

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

typedef int (*treedb_callback_t)(
    void *user_data,
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *operation,  // "EV_TREEDB_NODE_UPDATED", "EV_TREEDB_NODE_DELETED"
    json_t *node            // owned
);

PUBLIC int treedb_set_callback(
    json_t *tranger,
    const char *treedb_name,
    treedb_callback_t treedb_callback,
    void *user_data
);

/***************************************************************************
    Return is NOT YOURS, pkey MUST be "id"
    WARNING This function don't load hook links.
    HACK IDEMPOTENT function

    pkey2s: 1) string -> only one simple key (name of pkey2 is the name of topic field
      TODO  2) dict   -> several types of keys -> { name of pkey2: pkey2 type and his fields}
                values of dict:
                        string: one simple key, same as 1)
                        list of strings: combined key


    Schema (jn_cols)
    ================

    Dictionary of
        $key: {
            "header":       string
            "fillspace":    integer
            "type":         enum
            "flag":         enum
            "default":      blob
        }

    "type" enum
    -----------

        "string"
        "integer"
        "object" or "dict"
        "array" or "list"
        "real"
        "boolean"
        "enum"
        "blob"

    "flag" enum
    -----------

        "persistent"    // implicit "readable"
        "required"
        "fkey"
        "hook"
        "uuid"
        "notnull"
        "wild"
        "rowid"
        "inherit"

                        // For use in gobj attributes
        "readable"      // Field readable by user
        "writable"      // Field writable by user, implicit "readable"
        "stats"         // field with stats, implicit "readable"
        "rstats"        // field with resettable stats, implicit "stats"
        "pstats"        // field with persistent stats, implicit "stats"

        "password"
        "email"
        "url"

 ***************************************************************************/
PUBLIC json_t *treedb_create_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *topic_version,
    const char *topic_tkey,
    json_t *topic_pkey2s, // owned, string or dict of string | [strings]
    json_t *jn_cols, // owned
    uint32_t snap_tag,
    BOOL create_schema
);

PUBLIC int treedb_close_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

PUBLIC int treedb_delete_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

PUBLIC json_t *treedb_list_treedb( // Return a list with treedb names
    json_t *tranger,
    json_t *kw
);

PUBLIC json_t *treedb_topics( //Return a list with topic names of the treedb
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_options // "dict" return list of dicts, otherwise return list of strings
);

PUBLIC size_t treedb_topic_size(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

/*------------------------------------*
 *      Utils
 *------------------------------------*/
PUBLIC json_t *_treedb_create_topic_cols_desc(void);
PUBLIC int parse_schema_cols( // Return 0 if ok or # of errors in negative
    json_t *cols_desc,  // not owned
    json_t *data        // owned
);
PUBLIC int parse_hooks(
    json_t *tranger
);

PUBLIC int current_snap_tag(
    json_t *tranger,
    const char *treedb_name
);

PUBLIC BOOL treedb_is_treedbs_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);
PUBLIC json_t *treedb_get_id_index( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);
PUBLIC json_t *treedb_topic_pkey2s( // Return MUST be decref, a dict with pkey2s
    json_t *tranger,
    const char *topic_name
);

/**rst**
    Return refs of fkeys of col_name field
**rst**/
PUBLIC json_t *treedb_node_up_refs(  // Return MUST be decref
    json_t *tranger,
    json_t *node,    // not owned
    const char *topic_name,
    const char *col_name,
    BOOL verbose
);

/**rst**
    Return array of `parent_id` of `value`  (refs: parent_topic_name^parent_id^hook_name)
**rst**/
PUBLIC json_t *treedb_beautiful_up_refs(  // Return MUST be decref
    json_t *value
);

PUBLIC int treedb_set_trace(BOOL set);

/*------------------------------------*
 *      Manage the tree's nodes
 *------------------------------------*/

/**rst**
    Create a new node
**rst**/
PUBLIC json_t *treedb_create_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw, // owned
    json_t *jn_options
);

/**rst**
    Direct saving to tranger.
    WARNING be care, must be a pure node.
    Tag __tag__ (user_flag) is inherited.
**rst**/
PUBLIC int treedb_save_node(
    json_t *tranger,
    json_t *node    // not owned
);

/**rst**
    Update the existing current node with fields of kw
    "create" create node if not exist
**rst**/
PUBLIC json_t *treedb_update_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,    // owned
    json_t *jn_options // owned, "create"
);

/**rst**
    "force" delete links. If there are links are not force then delete_node will fail
**rst**/
PUBLIC int treedb_delete_node(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,    // owned (WARNING only 'id' field is used to find the node to delete)
    json_t *jn_options // bool "force"
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

/**rst**
    Return a list of matched nodes

    Meaning of parent and child 'references' (fkeys, hooks)
    -----------------------------------------------------
    'fkey ref'
        The parent's references (link to up) have 3 ^ fields:

            "topic_name^id^hook_name"

        WARNING: Parents references are never return with full node,
        to get parent data you must to access to the parent node.
        It's a hierarchy structure: Full access to childs, not to parents.

    'hook ref'
        The child's references (link to down) have 2 ^ fields:

            "topic_name^id"

    Options
    -------
    "collapsed"
        Yes:
            return a hook list in 'fkey ref' mode
            WARNING (always a **list**, not the original string/dict/list)
        No:
            Return hooks with full and original (string,dict,list) child's nodes.

    "only-fkey-id"
        Valid in collapsed and collapsed mode.
        Return the 'fkey ref' with only the 'id' field

    "only-hook-id"
        WARNING: Implicit collapsed mode.
        Return the 'hook ref' with only the 'id' field


    HACK id is converted in ids (using kwid_get_ids())
    HACK if __filter__ exists in jn_filter it will be used as filter

**rst**/
PUBLIC json_t *treedb_list_nodes( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_filter,  // owned
    json_t *jn_options, // owned "collapsed" "only-fkey-id" "only-hook-id"
    BOOL (*match_fn) (
        json_t *topic_desc, // not owned
        json_t *node,       // not owned
        json_t *jn_filter   // not owned
    )
);
PUBLIC json_t *treedb_node_instances( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name,
    json_t *jn_filter,  // owned
    json_t *jn_options, // owned, "collapsed" "only-fkey-id" "only-hook-id"
    BOOL (*match_fn) (
        json_t *topic_desc, // not owned
        json_t *node,       // not owned
        json_t *jn_filter   // not owned
    )
);

PUBLIC json_t *treedb_get_node( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *id,
    json_t *jn_options // owned, "collapsed" "only-fkey-id" "only-hook-id"
);

/*
 *  Return a list of parent nodes pointed by the link (fkey)
 */
PUBLIC json_t *treedb_list_parents( // Return MUST be decref
    json_t *tranger,
    const char *link, // must be a fkey field
    json_t *node, // not owned
    json_t *jn_options // owned, "collapsed"
);

/*
 *  Return number of parent nodes pointed by the link (fkey)
 */
PUBLIC size_t treedb_parents_size(
    json_t *tranger,
    const char *link, // must be a fkey field
    json_t *node // not owned
);

/*
 *  Return a list of child nodes of the hook (WARNING ONLY first level)
 */
PUBLIC json_t *treedb_list_childs(
    json_t *tranger,
    const char *hook,
    json_t *node, // not owned
    json_t *jn_options // owned, "collapsed"
);

/*
 *  Return number of child nodes of the hook (WARNING ONLY first level)
 */
PUBLIC size_t treedb_childs_size(
    json_t *tranger,
    const char *hook,
    json_t *node // not owned
);

/*----------------------------*
 *          Schema
 *----------------------------*/
/*
 *  Return a list with the cols that are links (fkeys) of the topic
 */
PUBLIC json_t *treedb_get_topic_links(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

/*
 *  Return a list with the cols that are hooks of the topic
 */
PUBLIC json_t *treedb_get_topic_hooks(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

/*----------------------------*
 *          Snaps
 *----------------------------*/
PUBLIC int treedb_shoot_snap( // tag the current tree db
    json_t *tranger,
    const char *treedb_name,
    const char *snap_name
);
PUBLIC int treedb_activate_snap( // Activate tag, return the snap tag
    json_t *tranger,
    const char *treedb_name,
    const char *snap_name
);
PUBLIC json_t *treedb_list_snaps( // Return MUST be decref, list of snaps
    json_t *tranger,
    const char *treedb_name,
    json_t *filter
);

#ifdef __cplusplus
}
#endif
