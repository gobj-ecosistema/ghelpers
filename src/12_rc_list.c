/***********************************************************************
 *        rc_list.c
 *
 *        Resource list: Double-Double link list functions.
 *        Independent functions of any C library.
 *
 *        Copyright (c) 2016 Niyamaka.
 *        All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "12_rc_list.h"

/*********************************************************
 *      Constants
 *********************************************************/

/***************************************************************
 *      If dl_list is null then create and return a dl_list
 ***************************************************************/
PUBLIC dl_list_t * rc_init_iter(dl_list_t *iter)
{
    if(!iter) {
        iter = gbmem_malloc(sizeof(dl_list_t));
        if(!iter) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "no memory for dl_list_t",
                "size",         "%d", sizeof(dl_list_t),
                NULL
            );
        }
    }
    memset(iter, 0, sizeof(dl_list_t));
    dl_init(iter);
    return iter;
}

/***************************************************************
 *  Free instances of list,
 *  and resource too if their instances is 0 and free_resource_fn are passed.
 ***************************************************************/
PUBLIC int rc_free_iter(dl_list_t *iter, BOOL free_iter, void (*free_resource_fn)(void *))
{
    if(!iter) {
        return -1;
    }
    rc_resource_t *resource; rc_instance_t *resource_i;
    while((resource_i=rc_first_instance(iter, &resource))) {
        rc_delete_instance(resource_i, free_resource_fn);
    }
    if(free_iter) {
        gbmem_free(iter);
    }
    return 0;
}

/***************************************************************
 *  Free instances of list,
 *  and resource too if their instances is 0 and free_resource_fn are passed.
 *  calling a callback with the next row to delete
 ***************************************************************/
PUBLIC int rc_free_iter2(
    dl_list_t *iter,
    BOOL free_iter,
    void (*free_resource_fn)(void *),
    int (*cb)(void *user_data, rc_resource_t *resource),
    void *user_data)
{
    if(!iter) {
        return -1;
    }
    rc_resource_t *resource; rc_instance_t *resource_i;
    while((resource_i=rc_first_instance(iter, &resource))) {
        if(cb) {
            (cb)(user_data, resource);
        }
        rc_delete_instance(resource_i, free_resource_fn);
    }
    if(free_iter) {
        gbmem_free(iter);
    }
    return 0;
}


/***************************************************************
 *  Get number of instances are in the iter list.
 ***************************************************************/
PUBLIC int rc_iter_size(dl_list_t *iter)
{
    // Yes, the iter is a dl_list.
    return dl_size(iter);
}

/***************************************************************
 *      Sort list by keys in jn_key_list list
 ***************************************************************/
PUBLIC int rc_sort(dl_list_t* dl_list, const char* key2sort)
{
    return 0; // TODO
}

/***************************************************************
 *      Add at end of list
 ***************************************************************/
PUBLIC rc_instance_t *rc_add_instance(dl_list_t *iter, rc_resource_t *resource, size_t resource_size)
{
    if(!resource) {
        resource = gbmem_malloc(sizeof(rc_resource_t) + resource_size);
        if(!resource) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "no memory for rc_resource_t",
                "size",         "%d", sizeof(rc_resource_t) + resource_size,
                NULL
            );
            return 0;
        }
        rc_init_iter(&resource->dl_instances);
        rc_init_iter(&resource->dl_childs);
    }

    /*
     *  Item for internal list
     */
    rc_instance_t *rc_instance_i = gbmem_malloc(sizeof(rc_instance_t));
    if(!rc_instance_i) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory for rc_instance_t",
            "size",         "%d", sizeof(rc_instance_t),
            NULL
        );
        return 0;
    }
    rc_instance_i->resource = resource;
    dl_add(&resource->dl_instances, rc_instance_i);

    /*
     *  Item for external list
     */
    rc_instance_t *rc_instance_e = gbmem_malloc(sizeof(rc_instance_t));
    if(!rc_instance_e) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory for rc_instance_t",
            "size",         "%d", sizeof(rc_instance_t),
            NULL
        );
        return 0;
    }
    rc_instance_e->resource = resource;
    dl_add(iter, rc_instance_e);

    /*
     *  Twins
     */
    rc_instance_i->twin_e = rc_instance_e;
    rc_instance_e->twin_i = rc_instance_i;

    /*
     *  External instance is for user
     */
    return rc_instance_e;
}

/***************************************************************
 *      insert at head of list
 ***************************************************************/
PUBLIC rc_instance_t *rc_insert_instance(dl_list_t *iter, rc_resource_t *resource, size_t resource_size)
{
    if(!resource) {
        resource = gbmem_malloc(sizeof(rc_resource_t) + resource_size);
        if(!resource) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "no memory for rc_resource_t",
                "size",         "%d", sizeof(rc_resource_t) + resource_size,
                NULL
            );
            return 0;
        }
        rc_init_iter(&resource->dl_instances);
        rc_init_iter(&resource->dl_childs);
    }

    /*
     *  Item for internal list
     */
    rc_instance_t *rc_instance_i = gbmem_malloc(sizeof(rc_instance_t));
    if(!rc_instance_i) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory for rc_instance_t",
            "size",         "%d", sizeof(rc_instance_t),
            NULL
        );
        return 0;
    }
    rc_instance_i->resource = resource;
    dl_add(&resource->dl_instances, rc_instance_i);

    /*
     *  Item for external list
     */
    rc_instance_t *rc_instance_e = gbmem_malloc(sizeof(rc_instance_t));
    if(!rc_instance_e) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory for rc_instance_t",
            "size",         "%d", sizeof(rc_instance_t),
            NULL
        );
        return 0;
    }
    rc_instance_e->resource = resource;
    dl_insert(iter, rc_instance_e);

    /*
     *  Twins
     */
    rc_instance_i->twin_e = rc_instance_e;
    rc_instance_e->twin_i = rc_instance_i;

    /*
     *  External instance is for user
     */
    return rc_instance_e;
}

/***************************************************************
 *  Add all resources of src_iter to dst_iter.
 ***************************************************************/
PUBLIC int rc_add_iter(dl_list_t *dst_iter, dl_list_t *src_iter)
{
    rc_resource_t * resource; rc_instance_t *i_hs;
    i_hs = rc_first_instance(src_iter, &resource);
    while(i_hs) {
        rc_add_instance(dst_iter, resource, 0);
        i_hs = rc_next_instance(i_hs, &resource);
    }
    return 0;
}

// /***************************************************************
//  *  Create clone iter
//  ***************************************************************/
// PUBLIC dl_list_t * rc_clone_iter(dl_list_t *src_iter)
// {
//     dl_list_t *clone = rc_init_iter(0);
//     rc_resource_t * resource; rc_instance_t *i_hs;
//     i_hs = rc_first_instance(src_iter, &resource);
//     while(i_hs) {
//         resource->refcount++;
//         rc_add_instance(clone, resource, 0);
//         i_hs = rc_next_instance(i_hs, &resource);
//     }
//
//     return clone;
// }
//
// /***************************************************************
//  *  Free with refcount
//  ***************************************************************/
// PRIVATE int rc_delete_instance2(rc_instance_t *rc_instance_e, void (*free_resource_fn)(void *))
// {
//     rc_resource_t *resource = rc_instance_e->resource;
//     rc_instance_t *rc_instance_i = rc_instance_e->twin_i;
//     dl_list_t *dl_instances = &resource->dl_instances;
//
//     /*
//      *  Check some pointers
//      */
//     if(!rc_instance_i) {
//         log_error(LOG_OPT_TRACE_STACK,
//             "gobj",         "%s", __FILE__,
//             "process",      "%s", get_process_name(),
//             "hostname",     "%s", get_host_name(),
//             "pid",          "%d", get_pid(),
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//             "msg",          "%s", "rc_instance_i NULL",
//             NULL
//         );
//         return -1;
//     }
//     if(!resource) {
//         log_error(LOG_OPT_TRACE_STACK,
//             "gobj",         "%s", __FILE__,
//             "function",     "%s", __FUNCTION__,
//             "process",      "%s", get_process_name(),
//             "hostname",     "%s", get_host_name(),
//             "pid",          "%d", get_pid(),
//             "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//             "msg",          "%s", "resource NULL",
//             NULL
//         );
//         return -1;
//     }
//     if(resource != rc_instance_i->resource) {
//         log_error(LOG_OPT_TRACE_STACK,
//             "gobj",         "%s", __FILE__,
//             "function",     "%s", __FUNCTION__,
//             "process",      "%s", get_process_name(),
//             "hostname",     "%s", get_host_name(),
//             "pid",          "%d", get_pid(),
//             "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//             "msg",          "%s", "resources internal and external DON'T MATCH",
//             NULL
//         );
//         return -1;
//     }
//     if(dl_instances != dl_dl_list(rc_instance_i)) {
//         log_error(LOG_OPT_TRACE_STACK,
//             "gobj",         "%s", __FILE__,
//             "function",     "%s", __FUNCTION__,
//             "process",      "%s", get_process_name(),
//             "hostname",     "%s", get_host_name(),
//             "pid",          "%d", get_pid(),
//             "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//             "msg",          "%s", "internals dl_list DON'T MATCH",
//             NULL
//         );
//         return -1;
//     }
//
//     /*
//      *  Delete the external instance
//      */
//     dl_delete_item(rc_instance_e, gbmem_free);
//
//     /*
//      *  Delete the internal instance
//      */
//     dl_delete_item(rc_instance_i, gbmem_free);
//
//     resource->refcount--;
//     if(resource->refcount == 0) {
//         if(free_resource_fn) {
//             free_resource_fn(resource);
//         }
//     }
//     return 0;
// }
//
// /***************************************************************
//  *  Free clone iter
//  ***************************************************************/
// PUBLIC int rc_free_clone_iter(dl_list_t *iter, void (*free_resource_fn)(void *))
// {
//     if(!iter) {
//         return -1;
//     }
//     rc_resource_t *resource; rc_instance_t *resource_i;
//     while((resource_i=rc_first_instance(iter, &resource))) {
//         rc_delete_instance2(resource_i, free_resource_fn);
//     }
//     gbmem_free(iter);
//     return 0;
// }

/***************************************************************
 *  Free instance,
 *  and resource with free_resource_fn if not null
 ***************************************************************/
PUBLIC int rc_delete_instance(rc_instance_t *rc_instance_e, void (*free_resource_fn)(void *))
{
    rc_resource_t *resource = rc_instance_e->resource;
    rc_instance_t *rc_instance_i = rc_instance_e->twin_i;
    dl_list_t *dl_instances = &resource->dl_instances;

    /*
     *  Check some pointers
     */
    if(!rc_instance_i) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "rc_instance_i NULL",
            NULL
        );
        return -1;
    }
    if(!resource) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "resource NULL",
            NULL
        );
        return -1;
    }
    if(resource != rc_instance_i->resource) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "resources internal and external DON'T MATCH",
            NULL
        );
        return -1;
    }
    if(dl_instances != dl_dl_list(rc_instance_i)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "internals dl_list DON'T MATCH",
            NULL
        );
        return -1;
    }

    /*
     *  Delete the external instance
     */
    dl_delete_item(rc_instance_e, gbmem_free);

    /*
     *  Delete the internal instance
     */
    dl_delete_item(rc_instance_i, gbmem_free);

    if(free_resource_fn && dl_size(dl_instances)<=0) {
        free_resource_fn(resource);
    }
    return 0;
}

/***************************************************************
 *  Free resource. All instance will be deleted.
 *  If free_resource is true then the resource too.
 ***************************************************************/
PUBLIC int rc_delete_resource(rc_resource_t* resource, void (*free_resource_fn)(void *))
{
    rc_instance_t *rc_instance_e;

    dl_list_t *dl_childs = &resource->dl_childs;
    while((rc_instance_e = dl_first(dl_childs))) {
        rc_delete_resource(rc_instance_e->resource, free_resource_fn);
    }

    rc_instance_t *rc_instance_i;
    dl_list_t *dl_instances = &resource->dl_instances;
    while((rc_instance_i = dl_first(dl_instances))) {
        rc_instance_t *rc_instance_e = rc_instance_i->twin_e;
        rc_delete_instance(rc_instance_e, FALSE);
    }

    if(free_resource_fn) {
        free_resource_fn(resource);
    }

    return 0;
}

/***************************************************************
 *  Get resource from instance
 ***************************************************************/
PUBLIC void *rc_get_resource(rc_instance_t *rc_instance)
{
    if(!rc_instance) {
        return 0;
    }
    return rc_instance->resource;
}

/***************************************************************
 *  Get number of instances of resource
 ***************************************************************/
PUBLIC size_t rc_instances_size(rc_resource_t *resource)
{
    dl_list_t *dl_instances = &resource->dl_instances;
    return dl_size(dl_instances);
}

/***************************************************************
 *  Set/Get id of instance
 *  You must reset id before set a new value
 ***************************************************************/
PUBLIC int rc_instance_set_id(rc_instance_t *rc_instance, uint64_t id)
{
    if(id && rc_instance->id) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "rc_instance id ALREADY SET",
            NULL
        );
        return -1;
    }
    rc_instance->id = id;
    return 0;
}
PUBLIC uint64_t rc_instance_get_id(rc_instance_t *rc_instance)
{
    return rc_instance->id;
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC rc_instance_t *rc_first_instance(dl_list_t *iter, rc_resource_t **resource)
{
    if(!iter) {
        if(resource) {
            *resource = 0;
        }
        return 0;
    }
    rc_instance_t *instance = dl_first(iter);
    if(!instance) {
        if(resource) {
            *resource = 0;
        }
        return 0;
    }
    if(resource) {
        *resource = rc_get_resource(instance);
    }
    return instance;
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC rc_instance_t *rc_last_instance(dl_list_t *iter, rc_resource_t **resource)
{
    rc_instance_t *instance = dl_last(iter);
    if(!instance) {
        if(resource) {
            *resource = 0;
        }
        return 0;
    }
    if(resource) {
        *resource =rc_get_resource(instance);
    }
    return instance;
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC rc_instance_t *rc_next_instance(rc_instance_t *current_instance, rc_resource_t **resource)
{
    rc_instance_t *instance = dl_next(current_instance);
    if(!instance) {
        if(resource) {
            *resource = 0;
        }
        return 0;
    }
    if(resource) {
        *resource =rc_get_resource(instance);
    }
    return instance;
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC rc_instance_t *rc_prev_instance(rc_instance_t *current_instance, rc_resource_t **resource)
{
    rc_instance_t *instance = dl_prev(current_instance);
    if(!instance) {
        if(resource) {
            *resource = 0;
        }
        return 0;
    }
    if(resource) {
        *resource =rc_get_resource(instance);
    }
    return instance;
}

/***************************************************************
 *
 ***************************************************************/
PUBLIC rc_instance_t *rc_instance_index(dl_list_t *iter, rc_resource_t *resource_, size_t *index_)
{
    size_t index = 0;
    rc_instance_t *instance = dl_first(iter);
    while(instance) {
        index++;    // relative to 1
        rc_resource_t *resource = rc_get_resource(instance);
        if(resource == resource_) {
            if(index_) {
                *index_ = index;
            }
            return instance;
        }
        instance = dl_next(instance);
    }
    if(index_) {
        *index_ = 0;
    }
    return 0; /* not found */
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC rc_instance_t *rc_instance_nfind(dl_list_t *iter, size_t index_, rc_resource_t** resource_)
{
    size_t index = 0;
    rc_instance_t *instance = dl_first(iter);
    while(instance) {
        index++;    // relative to 1
        if(index == index_) {
            rc_resource_t *resource = rc_get_resource(instance);
            if(resource_) {
                *resource_ = resource;
            }
            return instance;
        }
        instance = dl_next(instance);
    }
    if(resource_) {
        *resource_ = 0;
    }
    return 0; /* not found */
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC int rc_walk_by_list(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3)
{
    rc_resource_t *resource; rc_instance_t *instance;
    if(walk_type & WALK_LAST2FIRST) {
        instance = dl_last(iter);
    } else {
        instance = dl_first(iter);
    }
    resource = rc_get_resource(instance);

    while(instance) {
        /*
         *  Get next item before calling callback.
         *  This let destroy the current item,
         *  but NOT more ones.
         */
        rc_resource_t *next; rc_instance_t *next_i;
        if(walk_type & WALK_LAST2FIRST) {
            next_i = dl_prev(instance);
        } else {
            next_i = dl_next(instance);
        }
        next = rc_get_resource(next_i);

        int ret = (cb_walking)(instance, resource, user_data, user_data2, user_data3);
        if(ret < 0) {
            return ret;
        }

        instance = next_i;
        resource = next;
    }

    return 0;
}

/***************************************************************
 *  Walking
 *  If cb_walking return negative, the iter will stop.
 *  Valid options:
 *      WALK_BYLEVEL:
 *          WALK_FIRST2LAST
 *          or
 *          WALK_LAST2FIRST
 ***************************************************************/
PRIVATE int rc_walk_by_level(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3)
{
    /*
     *  First my childs
     */
    int ret = rc_walk_by_list(iter, walk_type, cb_walking, user_data, user_data2, user_data3);
    if(ret < 0) {
        return ret;
    }

    /*
     *  Now child's childs
     */
    rc_resource_t *resource; rc_instance_t *instance;
    if(walk_type & WALK_LAST2FIRST) {
        instance = dl_last(iter);
    } else {
        instance = dl_first(iter);
    }
    resource = rc_get_resource(instance);

    while(instance) {
        /*
         *  Get next item before calling callback.
         *  This let destroy the current item,
         *  but NOT more ones.
         */
        rc_resource_t *next; rc_instance_t *next_i;
        if(walk_type & WALK_LAST2FIRST) {
            next_i = dl_prev(instance);
        } else {
            next_i = dl_next(instance);
        }
        next = rc_get_resource(next_i);

        dl_list_t *dl_child_list = &resource->dl_childs;
        int ret = rc_walk_by_level(dl_child_list, walk_type, cb_walking, user_data, user_data2, user_data3);
        if(ret < 0) {
            return ret;
        }

        instance = next_i;
        resource = next;
    }

    return ret;
}

/***************************************************************
 *  Walking
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
 ***************************************************************/
PUBLIC int rc_walk_by_tree(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3)
{
    if(walk_type & WALK_BYLEVEL) {
        return rc_walk_by_level(iter, walk_type, cb_walking, user_data, user_data2, user_data3);
    }

    rc_resource_t *resource; rc_instance_t *instance;
    if(walk_type & WALK_BOTTOM2TOP) {
        instance = dl_last(iter);
    } else {
        instance = dl_first(iter);
    }
    resource = rc_get_resource(instance);

    while(instance) {
        /*
         *  Get next item before calling callback.
         *  This let destroy the current item,
         *  but NOT more ones.
         */
        rc_resource_t *next; rc_instance_t *next_i;
        if(walk_type & WALK_BOTTOM2TOP) {
            next_i = dl_prev(instance);
        } else {
            next_i = dl_next(instance);
        }
        next = rc_get_resource(next_i);

        if(walk_type & WALK_BOTTOM2TOP) {
            dl_list_t *dl_child_list = &resource->dl_childs;
            int ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data, user_data2, user_data3);
            if(ret < 0) {
                return ret;
            }

            ret = (cb_walking)(instance, resource, user_data, user_data2, user_data3);
            if(ret < 0) {
                return ret;
            }
        } else {
            int ret = (cb_walking)(instance, resource, user_data, user_data2, user_data3);
            if(ret < 0) {
                return ret;
            } else if(ret == 0) {
                dl_list_t *dl_child_list = &resource->dl_childs;
                ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data, user_data2, user_data3);
                if(ret < 0) {
                    return ret;
                }
            } else {
                // positive, continue next
            }
        }

        instance = next_i;
        resource = next;
    }

    return 0;
}





                    /*---------------------------------*
                     *            Childs
                     *---------------------------------*/




/***************************************************************
 *      Add child
 ***************************************************************/
PUBLIC rc_instance_t *rc_add_child(rc_resource_t *parent, rc_resource_t *child, size_t resource_size)
{
    if(!child) {
        child = gbmem_malloc(sizeof(rc_resource_t) + resource_size);
        if(!child) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "no memory for rc_resource_t",
                "size",         "%d", sizeof(rc_resource_t) + resource_size,
                NULL
            );
            return 0;
        }
    }

    /*
     *  Item for internal list
     */
    rc_instance_t *rc_instance_i = gbmem_malloc(sizeof(rc_instance_t));
    if(!rc_instance_i) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory for rc_instance_t",
            "size",         "%d", sizeof(rc_instance_t),
            NULL
        );
        return 0;
    }
    rc_instance_i->resource = child;
    dl_add(&child->dl_instances, rc_instance_i);

    /*
     *  Item for parent's child list
     */
    rc_instance_t *rc_instance_e = gbmem_malloc(sizeof(rc_instance_t));
    if(!rc_instance_e) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory for rc_instance_t",
            "size",         "%d", sizeof(rc_instance_t),
            NULL
        );
        return 0;
    }
    rc_instance_e->resource = child;
    dl_add(&parent->dl_childs, rc_instance_e);

    /*
     *  Direct access to parent
     */
    if(child->__parent__) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "child with parent",
            NULL
        );
    }
    child->__parent__ = parent;

    /*
     *  Twins
     */
    rc_instance_i->twin_e = rc_instance_e;
    rc_instance_e->twin_i = rc_instance_i;

    /*
     *  External instance is for user
     */
    return rc_instance_e;
}

/***************************************************************
 *  Remove child from parent.
 *  If free_resource is true then the resource too.
 ***************************************************************/
PUBLIC int rc_remove_child(rc_resource_t *parent, rc_resource_t *child, void (*free_child_fn)(void *))
{
    /*
     *  Find the instance of parent in child
     */
    size_t index;
    rc_instance_t *rc_parent_instance = rc_child_index(parent, child, &index);
    if(!rc_parent_instance) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parent's child instance NOT FOUND",
            NULL
        );
        return -1;
    }

    /*
     *  Direct access to parent
     */
    if(child->__parent__ != parent) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "child's parents DON'T MATCH",
            NULL
        );
    }
    child->__parent__ = 0;

    return rc_delete_instance(rc_parent_instance, free_child_fn);
}

/***************************************************************
 *  Free all childs.
 *  If free_resource is true then the childs memory too.
 ***************************************************************/
PUBLIC int rc_delete_all_childs(rc_resource_t *resource, void (*free_child_fn)(void *))
{
    dl_list_t *dl_childs = &resource->dl_childs;

    rc_instance_t *rc_instance;
    while((rc_instance = dl_first(dl_childs))) {
        rc_resource_t *resource_ = rc_get_resource(rc_instance);
        rc_delete_resource(resource_, free_child_fn);
    }

    return 0;
}

/***************************************************************
 *  Get number of childs
 ***************************************************************/
PUBLIC size_t rc_childs_size(rc_resource_t *resource)
{
    dl_list_t *dl_childs = &resource->dl_childs;
    return dl_size(dl_childs);
}

/***************************************************************
 *
 ***************************************************************/
PUBLIC rc_instance_t *rc_child_index(rc_resource_t *parent, rc_resource_t *child, size_t *index)
{
    rc_instance_t *instance = rc_instance_index(&parent->dl_childs, child, index);
    return instance;
}

/***************************************************************
 *  rc_child_nfind():
 *      Busca desde head por número, relative to 1
 ***************************************************************/
PUBLIC rc_instance_t *rc_child_nfind(rc_resource_t *parent, size_t index, rc_resource_t **child)
{
    rc_instance_t *instance = rc_instance_nfind(&parent->dl_childs, index, child);
    return instance;
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC int rc_walk_by_childs(
    rc_resource_t *resource,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3)
{
    return rc_walk_by_list(&resource->dl_childs, walk_type, cb_walking, user_data, user_data2, user_data3);
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC int rc_walk_by_childs_tree(
    rc_resource_t *resource,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3)
{
    return rc_walk_by_tree(&resource->dl_childs, walk_type, cb_walking, user_data, user_data2, user_data3);
}
