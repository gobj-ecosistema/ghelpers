/****************************************************************************
 *          GROWBUFFER.H
 *
 *          Buffer growable with limit
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <string.h>
#include "10_glogger.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************
 *              Structures
 **************************************************/
typedef struct {
    size_t allocated;   // allocated size in bf
    size_t len;         // current len of data in bf
    char *bf;
} grow_buffer_t;

/***************************************************
 *              Prototypes
 **************************************************/
/**rst**
    Initialize grow buffer system
**rst**/
PUBLIC int init_growbf(
    size_t max_size,
    void * (*malloc_fn)(size_t size),
    void * (*realloc_fn)(void *ptr, size_t size),
    void (*free_fn)(void *ptr)
);

/**rst**
    Alloc size. ``size`` cannot be greater than ``max_size``.
**rst**/
PUBLIC int growbf_ensure_size(
    grow_buffer_t *grbf,
    size_t size
);

/**rst**
    Free bf memory and set len to 0.
**rst**/
PUBLIC void growbf_reset(grow_buffer_t *grbf);

/**rst**
    Copy data to bf
**rst**/
PUBLIC int growbf_copy(grow_buffer_t *grbf, void *bf, size_t len);

#ifdef __cplusplus
}
#endif
