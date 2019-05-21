/***********************************************************************
 *          GROWBUFFER.C
 *
 *          Buffer growable with limit
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include "11_growbuffer.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE size_t __max_size__ = 64*1024L;
PRIVATE void * (*__malloc__)(size_t size) = malloc;
PRIVATE void * (*__realloc__)(void *ptr, size_t size) = realloc;
PRIVATE void (*__free__)(void *ptr) = free;

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int init_growbf(
    size_t max_size,
    void * (*malloc_fn)(size_t size),
    void * (*realloc_fn)(void *ptr, size_t size),
    void (*free_fn)(void *ptr))
{
    if(max_size>0) {
        __max_size__ = max_size;
    }
    __malloc__ = malloc_fn;
    __realloc__ = realloc_fn;
    __free__ = free_fn;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int growbf_ensure_size(
    grow_buffer_t *grbf,
    size_t size)
{
    if(grbf->allocated < size) {
        grbf->allocated = size;
        if(grbf->allocated > __max_size__) {
            grbf->allocated = __max_size__;
        }
        grbf->bf = __realloc__(grbf->bf, grbf->allocated);
        if (!grbf->bf) {
            log_error(LOG_OPT_ABORT,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "no memory for growbf",
                "size",         "%d", (int)grbf->allocated,
                NULL
            );
            return -1;
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void growbf_reset(grow_buffer_t *grbf)
{
    if(grbf->bf) {
        __free__(grbf->bf);
        grbf->bf = 0;
    }
    grbf->allocated = 0;
    grbf->len = 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int growbf_copy(grow_buffer_t *grbf, void *bf, size_t len)
{
    if(len > grbf->allocated) {
        len = grbf->allocated;
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "len greater than allocated",
            "len",          "%d", (int)len,
            "allocated",    "%d", (int)grbf->allocated,
            NULL
        );
    }
    memcpy(grbf->bf,  bf, len);

    return 0;
}

