/***********************************************************************
 *              GBMEM.H
 *              Block Memory Core
 *              Copyright (c) 1996-2013 Niyamaka.
 *              All Rights Reserved.
 ***********************************************************************/

#ifndef _GBMEM_H
#define _GBMEM_H 1

/*
 *  Dependencies
 */
#include "01_print_error.h"
#include "02_dl_list.h"
#include "10_glogger.h"

#ifdef __cplusplus
extern "C"{
#endif


/**************************************************************
 *       Constants
 **************************************************************/
typedef void * (*sys_malloc_fn)(size_t sz);
typedef void (*sys_free_fn)(void * ptr);
typedef void * (*sys_realloc_fn)(void * ptr, size_t sz);
typedef void (*sys_alarm_fn)(void);

/** A structure to specify system memory allocation functions to be used. */
typedef struct {
    sys_malloc_fn sys_malloc;
    sys_free_fn sys_free;
    sys_realloc_fn sys_realloc;
    sys_alarm_fn sys_alarm;     // called when no new superblock available
} sys_alloc_funcs_t;

/*
 * Examples of sys_alloc_funcs.
 * For Python:
#include <Python.h>
sys_alloc_funcs fns = {
    PyMem_Malloc,
    PyMem_Free,
    PyMem_Realloc
};

 * For normal use:
#include <stdlib.h>
sys_alloc_funcs fns = {
    malloc,
    free,
    realloc
};
*/

/*
 *  Helpers
 */
#define GBMEM_STR_DUP(dst, src)     \
    if(dst) {                       \
        gbmem_free((void *)(dst));  \
        (dst) = 0;                  \
    }                               \
    if(src) {                       \
        (dst) = gbmem_strdup(src);  \
    }

#define GBMEM_STR_NDUP(dst, src, len)           \
    if(dst) {                                   \
        gbmem_free((void *)(dst));              \
        (dst) = 0;                              \
    }                                           \
    if(src) {                                   \
        (dst) = gbmem_strndup((src), (len));    \
    }

#define GBMEM_MALLOC(dst, size)     \
    if(dst) {                       \
        gbmem_free((void *)(dst));  \
        dst = 0;                    \
    }                               \
    dst = gbmem_malloc(size);

#define GBMEM_FREE(ptr)             \
    if((ptr)) {                     \
        gbmem_free((void *)(ptr));  \
        (ptr) = 0;                  \
    }

#define CHECK_GBMEM_MALLOC_RETURN(ptr)                      \
    if(!(ptr)) {                                            \
        log_error(0,                                        \
            "gobj",         "%s", gobj_full_name(gobj),     \
            "function",     "%s", __FUNCTION__,             \
            "msgset",       "%s", MSGSET_MEMORY_ERROR,       \
            "msg",          "%s", "gbmem_malloc() FAILED",  \
            NULL                                            \
        );                                                  \
        return -1;                                          \
    }



/**************************************************************
 *       Prototypes
 **************************************************************/
PUBLIC int gbmem_startup(
    size_t min_block,                   /* smaller memory block */
    size_t max_block,                   /* largest memory block */
    size_t superblock,                  /* super-block size */
    size_t max_system_memory,           /* maximum core memory */
    sys_alloc_funcs_t *sys_alloc_funcs,   /* system memory functions */
    int flags
);
PUBLIC int gbmem_startup_system(
    size_t max_block,                       /* largest memory block */
    size_t max_system_memory                /* maximum system memory */
);

PUBLIC void gbmem_shutdown(void);

PUBLIC void gbmem_set_alarm(sys_alarm_fn sys_alarm);

/*
 *    Using mcore
 */
PUBLIC PTR    gbmem_malloc(size_t size);
PUBLIC void   gbmem_free(PTR ptr);
PUBLIC PTR    gbmem_realloc(PTR ptr, size_t size);
PUBLIC PTR    gbmem_calloc(size_t nmemb, size_t size);
PUBLIC char * gbmem_strdup(const char *str);  // duplicate a string
PUBLIC char * gbmem_strndup(const char *str, size_t n);

PUBLIC size_t gbmem_get_maximum_block(void);


/*
 *    Statistics
 */
PUBLIC void gbmem_stats(
    size_t *free_superblock_mem,
    size_t *free_segmented_mem,
    size_t *total_free_mem,
    size_t *allocated_system_mem,
    size_t *mem_in_use,
    size_t *cur_sm, // current system memory
    size_t *max_sm  // maximum system memory
);
PUBLIC size_t gbmem_mem_in_use(void);

PUBLIC void gbmem_trace_alloc_free( // You need to compile with ghelpersd (trace version)
    BOOL enable,
    uint32_t *memory_check_list
);
PUBLIC void gbmem_log_info(BOOL final);    // log_info of memory stats
PUBLIC void *gbmem_json_info(BOOL display_free_segmented_mem);        // return an json object with memory stats

PUBLIC void gbmem_enable_log_info(BOOL enable);


#ifdef __cplusplus
}
#endif

#endif /* _GBMEM_H */
