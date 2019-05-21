/***********************************************************************
 *        rc_list.h
 *
 *        Resource list: Double-Double link list functions.
 *        Independent functions of any C library.
 *
 *        Copyright (c) 2016 Niyamaka.
 *        All Rights Reserved.
 ***********************************************************************/

#ifndef rc_list_s_H
#define rc_list_s_H 1

#include <stddef.h>
/*
 *  Dependencies
 */
#include "01_print_error.h"
#include "02_dl_list.h"
#include "11_gbmem.h"

#ifdef __cplusplus
extern "C"{
#endif


/*****************************************************************
 *     Constants
 *****************************************************************/
typedef enum {
    /*
     * One must be selected
     */
    WALK_TOP2BOTTOM = 0x0001,
    WALK_BOTTOM2TOP = 0x0002,
    WALK_BYLEVEL    = 0x0004,

    /*
     * One must be selected when WALK_BYLEVEL is selected.
     */
    WALK_FIRST2LAST = 0x0010,
    WALK_LAST2FIRST = 0x0020,
} walk_type_t;


/*****************************************************************
 *     Structures
 *****************************************************************/
/*
    Make your resource typedef with this schema:

typedef struct myresource_s {
    struct myresource_s *__parent__;
    dl_list_t dl_instances;
    dl_list_t dl_childs;
    size_t refcount;

    // user data
    my data;
} myresource_t;

 */

#define RC_RESOURCE_HEADER              \
    struct rc_resource_s *__parent__;   \
    dl_list_t dl_instances;             \
    dl_list_t dl_childs;                \
    size_t refcount;

// Better make your resource typedef with this schema
typedef struct rc_resource_s {
    RC_RESOURCE_HEADER
    // user data
} rc_resource_t;

typedef struct rc_instance_s {
    DL_ITEM_FIELDS
    struct rc_instance_s *twin_e;
    struct rc_instance_s *twin_i;
    rc_resource_t *resource;
    uint64_t id;
} rc_instance_t;

typedef int (*cb_walking_t)(
    rc_instance_t *instance,
    rc_resource_t *resource,
    void *user_data,
    void *user_data2,
    void *user_data3
);

/*****************************************************************
 *     Prototypes
 *****************************************************************/
/*
 *  If dl_list is null then create and return a new dl_list (must be free with gbmem_free)
 */
PUBLIC dl_list_t *rc_init_iter(dl_list_t *iter);

/*
 *  Free instances of list,
 *  and resource too if their instances is 0 and free_resource_fn are passed.
 */
PUBLIC int rc_free_iter(dl_list_t *iter, BOOL free_iter, void (*free_resource_fn)(void *));

/*
 *  Free instances of list,
 *  and resource too if their instances is 0 and free_resource_fn are passed.
 *  calling a callback with the next row to delete
 */
PUBLIC int rc_free_iter2(
    dl_list_t *iter,
    BOOL free_iter,
    void (*free_resource_fn)(void *),
    int (*cb)(void *user_data, rc_resource_t *resource),
    void *user_data
);

/*
 *  Get number of instances are in the iter list.
 */
PUBLIC int rc_iter_size(dl_list_t *iter); // as dl_size()

/*
 *  Add at end of list
 *  If resource is NULL then alloc 'resource_size' memory for resource
 */
PUBLIC rc_instance_t *rc_add_instance(dl_list_t *iter, rc_resource_t *resource, size_t resource_size);

/*
 *  Insert at head of list
 *  If resource is NULL then alloc 'resource_size' memory for resource
 */
PUBLIC rc_instance_t *rc_insert_instance(dl_list_t *iter, rc_resource_t *resource, size_t resource_size);

/*
 *  Add all resources of src_iter to dst_iter.
 */
PUBLIC int rc_add_iter(dl_list_t *dst_iter, dl_list_t *src_iter);

/*
 *  Free instance, and resource with free_resource_fn if not null
 */
PUBLIC int rc_delete_instance(rc_instance_t *rc_instance, void (*free_resource_fn)(void *));

/*
 *  Free resource.
 *  All instances and childs will be deleted.
 *  If free_resource is not null then the resource too.
 */
PUBLIC int rc_delete_resource(rc_resource_t *resource, void (*free_resource_fn)(void *));

/*
 *  Get resource from instance
 */
PUBLIC void *rc_get_resource(rc_instance_t *rc_instance);

/*
 *  Get number of instances of resource
 */
PUBLIC size_t rc_instances_size(rc_resource_t *resource);

/*
 *  Set/Get id of instance
 *  You must reset id before set a new value
 */
PUBLIC int rc_instance_set_id(rc_instance_t *rc_instance, uint64_t id);
PUBLIC uint64_t rc_instance_get_id(rc_instance_t *rc_instance);


/*
 *  Walk over instances
 *      dl_list_t *iter;
 *      rc_instance_t *i_rc;
 *      rc_resource_t **resource
 */
#define rc_iter_foreach_forward(iter, i_rc, resource) \
    for(i_rc = rc_first_instance((iter), (rc_resource_t **)&(resource)); \
        i_rc!=0 ; \
        i_rc = rc_next_instance(i_rc, (rc_resource_t **)&(resource)))

#define rc_iter_foreach_backward(iter, i_rc, resource) \
    for(i_rc = rc_last_instance((iter), (rc_resource_t **)&(resource)); \
        i_rc!=0 ; \
        i_rc = rc_prev_instance((i_rc), (rc_resource_t **)&(resource)))

/*
 *  Example

    void *resource; rc_instance_t *i_rc;
    rc_foreach_foreach(iter, i_rc, resource) {
    }

*/

PUBLIC rc_instance_t *rc_first_instance(dl_list_t *iter, rc_resource_t **resource);
PUBLIC rc_instance_t *rc_last_instance(dl_list_t *iter, rc_resource_t **resource);
PUBLIC rc_instance_t *rc_next_instance(rc_instance_t *current_instance, rc_resource_t **resource);
PUBLIC rc_instance_t *rc_prev_instance(rc_instance_t *current_instance, rc_resource_t **resource);

/*
 *  rc_instance_index():
 *      Return the instance of `resource` in iter.
 *      If `index` is not null, find the index of `resource`, relative to 1
 */
PUBLIC rc_instance_t *rc_instance_index(dl_list_t *iter, rc_resource_t *resource, size_t *index);
/*
 *  rc_instance_nfind():
 *      Get the `resource` in `index` position (relative to 1)
 */
PUBLIC rc_instance_t *rc_instance_nfind(dl_list_t *iter, size_t index, rc_resource_t** resource);

/*
 *  If cb_walking return negative, the iter will stop.
 *  Valid options:
 *      WALK_FIRST2LAST
 *      or
 *      WALK_LAST2FIRST
 */
PUBLIC int rc_walk_by_list(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3
);
/*
 *  If cb_walking return negative, the iter will stop.
 *                return 0, continue
 *                return positive, step current branch (only in WALK_TOP2BOTTOM)
 *  Valid options:
 *      WALK_TOP2BOTTOM
 *      or
 *      WALK_BOTTOM2TOP
 *      or
 *      WALK_BYLEVEL:
 *          WALK_FIRST2LAST
 *          or
 *          WALK_LAST2FIRST
 */
PUBLIC int rc_walk_by_tree(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3
);


/*-------------------------------------*
 *          Childs
 *-------------------------------------*/

/*
 *  Add child
 *  If resource is NULL then alloc 'resource_size' memory for resource
 */
PUBLIC rc_instance_t *rc_add_child(rc_resource_t *parent, rc_resource_t *child, size_t resource_size);

/*
 *  Remove child from parent
 *  If free_resource_fn is not null then
 *      if the resource has reached 0 instances then
 *          instance will be free with free_resource_fn.
 */
PUBLIC int rc_remove_child(rc_resource_t *parent, rc_resource_t *child, void (*free_resource_fn)(void *));

/*
 *  Free all childs. If free_child_fn is not null then the childs memory too.
 */
PUBLIC int rc_delete_all_childs(rc_resource_t *resource, void (*free_child_fn)(void *));

PUBLIC size_t rc_childs_size(rc_resource_t *resource); // get number of childs


PUBLIC rc_instance_t *rc_child_index(rc_resource_t *parent, rc_resource_t *child, size_t *index); // relative to 1
/*
 *  rc_child_nfind():
 *      Busca desde head por número, relative to 1
 */
PUBLIC rc_instance_t *rc_child_nfind(rc_resource_t* parent, size_t index, rc_resource_t** child);

PUBLIC int rc_walk_by_childs(
    rc_resource_t *resource,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3
);
PUBLIC int rc_walk_by_childs_tree(
    rc_resource_t *resource,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3
);

#ifdef __cplusplus
}
#endif

#endif
