/***********************************************************************
 *              GBMEM.C
 *              Block Memory Core
 *              Copyright (c) 1996-2013 Niyamaka.
 *              All Rights Reserved.
 *
 *  Code inspired in zmalloc.c (MAWK)
 *  copyright 1991,1993, Michael D. Brennan
 *  Mawk is distributed without warranty under the terms of
 *  the GNU General Public License, version 2, 1991.
 *
 *   ZBLOCKS of sizes 1, 2, 3, ... 128
 *   are stored on the linked linear lists in
 *   pool[0], pool[1], pool[2],..., pool[127]
 *
 *   Para minimum size of 128 y pool size de 32 (pool[32]) ->
 *        sizes 128, 256, 384, 512, ... 2048,...,4096
 *   Para minimum size of 16 y pool size de 256 (pool[256]) ->
 *        sizes 16, 32, 48, 64,... 2048,...,4096
 *
 ***********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <jansson.h> /* by gbmem_json_info */
#include <uv.h> /* by mutex */
#include "11_gbmem.h"

/***************************************************************
 *          Constants
 ***************************************************************/
#define EXTRA_MEM 0  // 32=OK, XXX

typedef uint32_t bdl_t;
typedef uint32_t memtrace_t;    // WARNING: hardcoded in some places.

#define MINIMUM_MIN_BLOCK (sizeof(bdl_t) + sizeof(memtrace_t))


/*
 *  Uncomment this line for trace alloc/free memory
 *  (set in ghelpersd)
 */
//#define __FULL_TRACE__ 1

/***************************************************************
 *          Structures
 ***************************************************************/
typedef union _zblock {
    union _zblock *link;
} ZBLOCK;

typedef struct sb_s {
    DL_ITEM_FIELDS

    void *pointer;
} sb_t;

/***************************************************************
 *          Data
 ***************************************************************/
#ifdef __FULL_TRACE__
PRIVATE memtrace_t __mem_trace_counter__ = 0;
#endif

PRIVATE char __maximum_superblocks_reached_informed = 0;

PRIVATE char __log_info_enabled__ = 0;
PRIVATE uint32_t *__memory_check_list__;
PRIVATE int __trace_allocs_frees__ = 0;
PRIVATE char __gbmem_initialized__ = FALSE;

/* original parameters, saved for info */
PRIVATE size_t __min_block__;                   /* smaller memory block */
PRIVATE size_t __max_block__ = 1024L*1024L;     /* largest memory block, default for no-using apps*/
PRIVATE size_t __superblock__;                  /* super-block size */
PRIVATE size_t __max_system_memory__ = 10*1024L*1024L;   /* maximum core memory, default for no-using apps */
PRIVATE size_t __cur_system_memory__ = 0;   /* current system memory */

PRIVATE size_t block_size;        /* size of the smaller memory block */
PRIVATE size_t pool_size;         /* size of pool */
PRIVATE bdl_t chunk;              /* number of blocks of block_size to get from memory system */
PRIVATE size_t maxsb;             /* nº máximo de superbloques */
PRIVATE size_t shift;             /* las veces que multiplicando 2 sale block_size */

PRIVATE sys_alloc_funcs_t __sys_alloc_funcs__ = {
    malloc,
    free,
    realloc,
    0
};

#define BytesToBlocks(size) (bdl_t)(((size) + block_size - 1) >> shift)
// static bdl_t BytesToBlocks(size_t size)
// {
//     size_t x = size + block_size - 1;
//     x = x >> shift;
//     bdl_t b = x;
//     return b;
// }

#define BlocksToBytes(size) ((size) << (shift))

/*
 *  Data needing to be protected.
 */
PRIVATE uv_mutex_t mutex_gbmem; // Always threadsafe. If can be used by other libraries using threads.

PRIVATE ZBLOCK  **pool;         /* pool of free block lists */
PRIVATE size_t *pool_sizes;     /* pool of free block list sizes */
PRIVATE bdl_t amt_avail;        /* nº bloques libres del superbloque actual */
PRIVATE ZBLOCK *avail;          /* pointer to avail memory in current superblock */
PRIVATE size_t count_sb;        /* nº superbloques allocated */

PRIVATE dl_list_t dl_superblocks;


/***************************************************************
 *          Prototypes
 ***************************************************************/
PRIVATE BOOL alloc_superblock(bdl_t req);
PRIVATE char * gbmem_substr_dup(const char *str, size_t size);


/***************************************************************************
 *    Startup gbmem
 *
 *    block_size    tamaño del bloque mas pequeño.
 *                  (Mínimo MINIMUM_MIN_BLOCK bytes).
 *
 *    pool_size     tamaño del array de bloques.
 *                  Define el bloque máximo:
 *                      bloque máximo = pool_size * block_size
 *
 *    chunk         número de bloques de memoria que se piden al sistema
 *                  cuando se agota el superbloque actual:
 *                      superbloque = chunk * block_size
 *
 *    maxsb         máximo número de superbloques que se pueden pedir al sistema.
 *
 *    Return -1 if error
 *
 ***************************************************************************/
PUBLIC int gbmem_startup(
    size_t min_block,                       /* smaller memory block */
    size_t max_block,                       /* largest memory block */
    size_t superblock,                      /* super-block size */
    size_t max_system_memory,               /* maximum core memory */
    sys_alloc_funcs_t *sys_alloc_funcs,     /* system memory functions */
    int flags
)
{
    size_t size;
    size_t t;

    if(__gbmem_initialized__)
        return -1;

    /*----------------------------------------------*
     *  Set a mutex
     *----------------------------------------------*/
    if (uv_mutex_init(&mutex_gbmem)) {
        log_error(LOG_OPT_ABORT|LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "uv_mutex_init() FAILED",
            NULL
        );
    }
    uv_mutex_lock(&mutex_gbmem);

    /*----------------------------------------------*
     *  Check block_size mininum
     *----------------------------------------------*/
    if(min_block <MINIMUM_MIN_BLOCK) {
        log_error(
            0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "min_block TOO SMALL",
            "min_block",    "%ld", (long)min_block,
            "MINIMUM_MIN_BLOCK", "%d", (int)MINIMUM_MIN_BLOCK,
            NULL
        );
        uv_mutex_unlock(&mutex_gbmem);
        print_error(
            PEF_EXIT,
            "ERROR YUNETA",
            "MIN_BLOCK too small: %d",
            min_block
        );
        return -1;
    }
    if(max_block % min_block != 0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "max_block MUST BE MULTIPLE of min_block",
            "min_block",    "%ld", (long)min_block,
            "max_block",    "%ld", (long)max_block,
            NULL
        );
        uv_mutex_unlock(&mutex_gbmem);
        print_error(
            PEF_EXIT,
            "ERROR YUNETA", "MAX_BLOCK %d not multiple of MIN_BLOCK %d",
            max_block,
            min_block
        );
        return -1;
    }
    if(superblock % min_block != 0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "superblock MUST BE MULTIPLE of min_block",
            "superblock",   "%ld", (long)superblock,
            "min_block",    "%ld", (long)min_block,
            NULL);
        uv_mutex_unlock(&mutex_gbmem);
        print_error(
            PEF_EXIT,
            "ERROR YUNETA", "MAX_BLOCK %d not multiple of MIN_BLOCK %d",
            max_block,
            min_block
        );
        return -1;
    }

    /*--------------------------------*
     *  Initialize
     *--------------------------------*/
    dl_init(&dl_superblocks);
    // save for info
    __min_block__ = block_size = min_block;
    __max_block__ = max_block;
    __superblock__ = superblock;
    __max_system_memory__ = max_system_memory;

    pool_size = max_block/min_block;
    chunk = superblock/min_block;
    if(!chunk) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "chunk is 0",
            NULL);
        uv_mutex_unlock(&mutex_gbmem);
        abort();
    }
    maxsb = max_system_memory/superblock;
    if(sys_alloc_funcs) {
        __sys_alloc_funcs__ = *sys_alloc_funcs;
    }

    /*----------------------------------------------*
     *  Info of startup
     *----------------------------------------------*/
    if(__trace_allocs_frees__) {
        log_debug(0,
            "gobj",                 "%s", __FILE__,
            "msgset",               "%s", MSGSET_STARTUP,
            "msg",                  "%s", "gbmem_startup",
            "MIN_BLOCK",            "%d", (int)min_block,
            "MAX_BLOCK",            "%d", (int)max_block,
            "SUPERBLOCK",           "%ld", (long)superblock,
            "MAX_SYSTEM_MEMORY",    "%ld", (long)max_system_memory,
            "block_size",           "%ld", (long)block_size,
            "pool_size",            "%ld", (long)pool_size,
            "chunk",                "%ld", (long)(size_t)chunk,
            "maxsb",                "%ld", (long)maxsb,
            NULL
        );
    }

    /*--------------------------------*
     *  Create pool
     *--------------------------------*/
    size = pool_size * sizeof(ZBLOCK *);
    pool = __sys_alloc_funcs__.sys_malloc(size);
    if(!pool) {
        log_critical(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot create pool, error sys_malloc()",
            "size",         "%ld", (long)size,
            NULL);
        uv_mutex_unlock(&mutex_gbmem);
        return -1;
    }
    memset(pool, 0, size);

    size = pool_size * sizeof(size_t);
    pool_sizes = __sys_alloc_funcs__.sys_malloc(size);
    if(!pool_sizes) {
        log_critical(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot create pool_sizes, error sys_malloc()",
            "size",         "%ld", (long)size,
            NULL
        );
        uv_mutex_unlock(&mutex_gbmem);
        return -1;
    }
    memset(pool_sizes, 0, size);

    /*
     *  Calculate shift: las veces que multiplicando 2 sale block_size
     */
    shift = 1;
    t = 2;
    while(t != block_size) {
        shift++;
        t = t*2;
    }

    /*--------------------------------*
     *      Alloc system memory
     *--------------------------------*/
    if(!alloc_superblock(0)) {
        log_critical(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "alloc_superblock() error",
            NULL);
        uv_mutex_unlock(&mutex_gbmem);
        return -1;
    }

    __gbmem_initialized__ = TRUE;
    uv_mutex_unlock(&mutex_gbmem);

    return 0;
}

/***************************************************************************
 *     Inicializa gestor memoria para usar memoria del sistema
 ***************************************************************************/
PUBLIC int gbmem_startup_system(
    size_t max_block,                       /* largest memory block */
    size_t max_system_memory)               /* maximum system memory */
{
    __max_block__ = max_block;
    __max_system_memory__ = max_system_memory;
    return 0;
}

/***************************************************************************
 *     Cierra gestor memoria
 ***************************************************************************/
PUBLIC void gbmem_shutdown(void)
{
    if(!__gbmem_initialized__) {
        if(__cur_system_memory__) {
            log_error(0,
                "gobj",                 "%s", __FILE__,
                "process",              "%s", get_process_name(),
                "hostname",             "%s", get_host_name(),
                "pid",                  "%d", get_pid(),
                "msgset",               "%s", MSGSET_STATISTICS,
                "msg",                  "%s", "shutdown: system memory not free",
                "cur_system_memory",    "%ld", (long)__cur_system_memory__,
                NULL
            );
        }
        return;
    }

    if(__log_info_enabled__) {
        log_debug(0,
            "gobj",                 "%s", __FILE__,
            "msgset",               "%s", MSGSET_STARTUP,
            "msg",                  "%s", "gbmem_shutdown",
            "count_sb",             "%ld", (long)count_sb,
            NULL);

        gbmem_log_info(TRUE);
        json_t *jn_info = gbmem_json_info(TRUE);
        log_debug_json(0, jn_info, "Segmented Mem");
        json_decref(jn_info);
    } else {
        size_t free_superblock_mem=0;
        size_t free_segmented_mem=0;
        size_t total_free_mem=0;
        size_t allocated_system_mem=0;
        size_t mem_in_use=0;
        size_t cur_sb;
        size_t max_sb;

        gbmem_stats(
            &free_superblock_mem,
            &free_segmented_mem,
            &total_free_mem,
            &allocated_system_mem,
            &mem_in_use,
            &cur_sb,
            &max_sb
        );
        if(mem_in_use) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_STATISTICS,
                "msg",          "%s", "shutdown: gbmem memory not free",
                "mem_in_use",   "%ld", (long)mem_in_use,
                NULL
            );
        }
    }

    uv_mutex_lock(&mutex_gbmem);

    block_size = 0;
    pool_size = 0;
    shift = 0;
    maxsb = 0;
    count_sb = 0;

    /*---------------------------------------*
     *    Free pool
     *---------------------------------------*/
    if(pool) {
        __sys_alloc_funcs__.sys_free(pool);
        pool = 0;
    }
    if(pool_sizes) {
        __sys_alloc_funcs__.sys_free(pool_sizes);
        pool_sizes = 0;
    }

    /*---------------------------------------*
     *    Free system memory blocks
     *---------------------------------------*/
    sb_t *sb = dl_first(&dl_superblocks);;
    while(sb) {
        if(sb->pointer) {
            __sys_alloc_funcs__.sys_free(sb->pointer);
            sb->pointer = 0;
        }
        sb = dl_next(sb);
    }
    dl_flush(&dl_superblocks, __sys_alloc_funcs__.sys_free);
    amt_avail = 0;
    avail = 0;
    chunk = 0;

    uv_mutex_unlock(&mutex_gbmem);
    uv_mutex_destroy(&mutex_gbmem);
    __gbmem_initialized__ = FALSE;
}

/***********************************************************************
 *  Set callback alarm function.
 *  Called when no new superblock is available.
 ***********************************************************************/
PUBLIC void gbmem_set_alarm(sys_alarm_fn sys_alarm)
{
    __sys_alloc_funcs__.sys_alarm = sys_alarm;
}

/***********************************************************************
 *      Alloc a superblock (system memory)
 *  req is the failed 'blocks' generating new memory
 ***********************************************************************/
PRIVATE BOOL alloc_superblock(bdl_t req)
{
    size_t total;

    if(count_sb >= maxsb) {
        if(!__maximum_superblocks_reached_informed) {
            // se repite muchisimo y bloque al VOS
            log_critical(
                0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "MAXIMUM SUPERBLOCKS REACHED",
                "maxsb",        "%ld", (long)maxsb,
                NULL
            );
            __maximum_superblocks_reached_informed = TRUE;
        }
        return FALSE;
    }

    if(__log_info_enabled__) {
        gbmem_log_info(FALSE);
    }

    if(amt_avail) { /* free avail */ // WARNING TODO
        --amt_avail;
        avail->link = pool[amt_avail];
        pool[amt_avail] = avail;
        pool_sizes[amt_avail]++;
    }

    total = (size_t)chunk * block_size;
    avail = (ZBLOCK *) __sys_alloc_funcs__.sys_malloc(total);
    if (!avail) {
        /* if we get here, almost out of memory */
        amt_avail = 0;
        log_warning(
            0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "ALMOST OUT OF MEMORY",
            "size",         "%ld", (long)((size_t)chunk * (size_t)block_size),
            NULL
        );
        print_error(
            PEF_EXIT,
            "ERROR YUNETA",
            "NO MORE MEMORY"
        );
        return FALSE;
    } else {
        amt_avail = chunk;
    }
    count_sb++;  /* cuenta el nº de superbloques */
    memset(avail, 0, total);

    /*
     *  Guarda el superblock en la lista de superbloques
     */
    total = sizeof(sb_t);
    sb_t *sb = __sys_alloc_funcs__.sys_malloc(total);
    if(!sb) {
        log_error(
            LOG_OPT_TRACE_STACK|LOG_OPT_EXIT_NEGATIVE,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "sys_malloc() FAILED",
            "size",         "%d", (int)total,
            NULL
        );
        return FALSE;
    }
    memset(sb, 0, total);
    sb->pointer = avail;
    dl_add(&dl_superblocks, sb);

    if(__log_info_enabled__) {
        log_debug(0,
            "gobj",         "%s", __FILE__,
            "msgset",       "%s", MSGSET_STATISTICS,
            "msg",          "%s", "new superblock",
            "pointer",      "%p", avail,
            "req_blocks",   "%d", (int)req,
            "sb",           "%d", (int)count_sb,
            "maxsb",        "%d", (int)maxsb,
            #ifdef __FULL_TRACE__
            "memtrace",     "%d", (int)__mem_trace_counter__,
            #endif
            "sb bytes",     "%ld", (long)((size_t)chunk * (size_t)block_size),
            "total bytes",  "%ld", (long)((size_t)count_sb * (size_t)chunk * (size_t)block_size),
            NULL
        );
    }

    return TRUE;
}

/***********************************************************************
 *      Alloc memory
 ***********************************************************************/
PUBLIC PTR gbmem_malloc(size_t size)
{
    register ZBLOCK *p ;
    bdl_t blocks;
    size_t bytes;
    #ifdef __FULL_TRACE__
    size_t original_size = size;
    #endif

    if(!__gbmem_initialized__) { // XXX
//         return calloc(1, size);

        if(size > __max_block__) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "SIZE GREATHER THAN MAX_BLOCK",
                "size",         "%ld", (long)size,
                "max_block",    "%d", (int)__max_block__,
                NULL
            );
            return (PTR)0;
        }
        size += EXTRA_MEM + sizeof(size_t);

        __cur_system_memory__ += size;

        if(__cur_system_memory__ > __max_system_memory__) {
            log_critical(0,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "process",              "%s", get_process_name(),
                "hostname",             "%s", get_host_name(),
                "pid",                  "%d", get_pid(),
                "msgset",               "%s", MSGSET_MEMORY_ERROR,
                "msg",                  "%s", "REACHED MAX_SYSTEM_MEMORY",
                "cur_system_memory",    "%ld", (long)__cur_system_memory__,
                "max_system_memory",    "%ld", (long)__max_system_memory__,
                NULL
            );
            abort();
        }
        char *pm = calloc(1, size);
        if(!pm) {
            log_critical(0,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "process",              "%s", get_process_name(),
                "hostname",             "%s", get_host_name(),
                "pid",                  "%d", get_pid(),
                "msgset",               "%s", MSGSET_MEMORY_ERROR,
                "msg",                  "%s", "NO MEMORY calloc() failed",
                "size",                 "%ld", (long)size,
                NULL
            );
            abort();
            return (PTR)0;
        }
        size_t *pm_ = (size_t*)pm;
        *pm_ = size;

        pm += sizeof(size_t);

        return pm;
    }

    if(size==0) {
        log_warning(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "size is zero",
            "size",         "%d", (int)size,
            NULL);
        return (PTR)0;
    }

    /*--------------------------------------------------*
     *  Calcula los bloques que corresponden a size
     *--------------------------------------------------*/
    size += sizeof(bdl_t); /* reserva para guardar el tamaño del bloque */

    #ifdef __FULL_TRACE__
    size += sizeof(memtrace_t);
    #endif

    if(size > __max_block__) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "SIZE GREATHER THAN MAX_BLOCK",
            "size",         "%ld", (long)size,
            "max_block",    "%d", (int)__max_block__,
            NULL
        );
        return (PTR)0;
    }
    blocks = BytesToBlocks(size);
    bytes = (size_t) BlocksToBytes(blocks);

    if (blocks > pool_size) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "BLOCK GREATER THAN POOL_SIZE",
            "size",         "%ld", (long)size,
            "blocks",       "%d", (int)blocks,
            NULL
        );
        return (PTR)0;
    } else if(blocks == 0) { // it happens when bdl_t is small for large values
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "blocks is ZERO",
            "size",         "%ld", (long)size,
            "blocks",       "%d", (int)blocks,
            NULL
        );
        return (PTR)0;
    }

    /*--------------------------------------------------*
     *  Lock
     *--------------------------------------------------*/
    uv_mutex_lock(&mutex_gbmem);

    #ifdef __FULL_TRACE__
    __mem_trace_counter__++;

    if(__trace_allocs_frees__) {
        for(int xx=0; __memory_check_list__ && __memory_check_list__[xx]!=0; xx++) {
            if(__mem_trace_counter__  == __memory_check_list__[xx]) {
                log_debug(LOG_OPT_TRACE_STACK,
                    "msgset",               "%s", MSGSET_STATISTICS,
                    "msg",                  "%s", "mem counter found",
                    "mem_trace_counter",    "%d", (int)__mem_trace_counter__,
                    NULL
                );
            }
            if(xx > 5)
                break;  // bit a bit please
        }
    }
    #endif

    /*--------------------------*
     *  Alloca el bloque
     *--------------------------*/
    p = pool[blocks-1];
    if(p) {
        pool[blocks-1] = p->link;
        pool_sizes[blocks-1]--;

        memset(p, 0, bytes);

        #ifdef __FULL_TRACE__
        /*
         *  Save trace
         */
        *((memtrace_t *)p) = __mem_trace_counter__;
        p = (ZBLOCK *)(((memtrace_t *)p) + 1);
        #endif

        /*
         *  Save size (in blocks)
         */
        *((bdl_t *)p) = blocks;
        p = (ZBLOCK *)(((bdl_t *)p) + 1);

        #ifdef __FULL_TRACE__
        if(__trace_allocs_frees__) {
            log_debug(0,
                "gobj",         "%s", __FILE__,
                "msgset",       "%s", MSGSET_STATISTICS,
                "msg",          "%s", "alloc memory",
                "memtrace",     "%d", (int)__mem_trace_counter__,
                "blocks",       "%d", (int)blocks,
                "type",         "%s", "from pool",
                "pointer",      "%p", p,
                "size",         "%ld", (long)size,
                "original_size","%ld", (long)original_size,
                NULL);
        }
        #endif

        /*--------------------------------------------------*
         *  Unlock
         *--------------------------------------------------*/
        uv_mutex_unlock(&mutex_gbmem);
        return (PTR) p ;
    }

    if(blocks > amt_avail) {
        if(!alloc_superblock(blocks)) {
// Se repite muchisimo y bloquea al VOS
//             log_warning(0,
//                 "gobj",         "%s", __FILE__,
//                 "function",     "%s", __FUNCTION__,
//                 "msgset",       "%s", MSGSET_MEMORY_ERROR,
//                 "msg",          "%s", "alloc_superblock() FAILED, trying upper block",
//                 "size",         "%ld", (long)size,
//                 "blocks",       "%d", (int)blocks,
//                 NULL
//             );

            /*---------------------------------------------------*
             *  Permite allocar bloques de tamaño superior
             *  (Se derrocha memoria y si ocurre con frecuencia
             *  malo).
             *---------------------------------------------------*/
            #ifdef __FULL_TRACE__
            bdl_t original_blocks = blocks;
            #endif
            while(blocks <= pool_size) {
                p = pool[blocks-1];
                if(p) {
                    pool[blocks-1] = p->link;
                    pool_sizes[blocks-1]--;

                    memset(p, 0, bytes);

                    #ifdef __FULL_TRACE__
                    /*
                     *  Save trace
                     */
                    *((memtrace_t *)p) = __mem_trace_counter__;
                    p = (ZBLOCK *)(((memtrace_t *)p) + 1);
                    #endif

                    /*
                     *  Save size (in blocks)
                     */
                    *((bdl_t *)p) = blocks;
                    p = (ZBLOCK *)(((bdl_t *)p) + 1);

                    #ifdef __FULL_TRACE__
                    if(__trace_allocs_frees__) {
                        log_debug(0,
                            "gobj",         "%s", __FILE__,
                            "msgset",       "%s", MSGSET_STATISTICS,
                            "msg",          "%s", "alloc memory",
                            "memtrace",     "%d", (int)__mem_trace_counter__,
                            "blocks",       "%d", (int)blocks,
                            "type",         "%s", "from higher pool",
                            "pointer",      "%p", p,
                            "size",         "%ld", (long)size,
                            "original_size","%ld", (long)original_size,
                            "orig-blocks",  "%d", (int)original_blocks,
                            NULL);
                    }
                    #endif

                    /*--------------------------------------------------*
                     *  Unlock
                     *--------------------------------------------------*/
                    uv_mutex_unlock(&mutex_gbmem);

                    /*--------------------------------------------------*
                     *  WARNING useful to enter in a freezed state.
                     *  Be careful with new memory allocs.
                     *--------------------------------------------------*/
                    if(__sys_alloc_funcs__.sys_alarm) {
                        static char done = 0;
                        if(!done) {
                            done = 1;
                            __sys_alloc_funcs__.sys_alarm();
                        }
                    }

                    return (PTR) p ;
                }
                blocks++;
            }
            log_critical(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "ALLOC_SUPERBLOCK FAILED and NO UPPER free block, ABORT!!",
                "size",         "%ld", (long)size,
                "blocks",       "%d", (int)blocks,
                NULL);
            // No es este el resumen que quiero gbmem_log_info(FALSE);
            abort();
        }
    }

    /* get p from the avail pile */
    p = avail;
    memset(p, 0, bytes);

    /* Descuenta la memoria usada del superbloque */
    avail = (ZBLOCK *) ((char *)avail + blocks * block_size);
    amt_avail -= blocks;

    #ifdef __FULL_TRACE__
    /*
     *  Save trace
     */
    *((memtrace_t *)p) = __mem_trace_counter__;
    p = (ZBLOCK *)(((memtrace_t *)p) + 1);
    #endif

    /*
     *  Save size (in blocks)
     */
    *((bdl_t *)p) = blocks;
    p = (ZBLOCK *)(((bdl_t *)p) + 1);

    #ifdef __FULL_TRACE__
    if(__trace_allocs_frees__) {
        log_debug(0,
            "gobj",         "%s", __FILE__,
            "msgset",       "%s", MSGSET_STATISTICS,
            "msg",          "%s", "alloc memory",
            "memtrace",     "%d", (int)__mem_trace_counter__,
            "blocks",       "%d", (int)blocks,
            "type",         "%s", "from avail",
            "pointer",      "%p", p,
            "size",         "%ld", (long)size,
            "original_size","%ld", (long)original_size,
            NULL);
    }
    #endif

    /*--------------------------------------------------*
     *  Unlock
     *--------------------------------------------------*/
    uv_mutex_unlock(&mutex_gbmem);

    return (PTR) p;
}

/***********************************************************************
 *      Free memory
 ***********************************************************************/
PUBLIC void gbmem_free(PTR p)
{
    bdl_t blocks;
#ifdef __FULL_TRACE__
    PTR pp = p;
#endif

    if(!p) {
        return; // El comportamiento como free() es que no salga error; lo quito por libuv (uv_try_write)
        /*
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "pointer is NULL",
            NULL
        );
        */
        return;
    }

    if(!__gbmem_initialized__) { // XXX
//         free(p); return;

        char *pm = p;
        pm -= sizeof(size_t);

        size_t *pm_ = (size_t*)pm;
        size_t size = *pm_;

        __cur_system_memory__ -= size;
        free(pm);
        return;
    }

    /*--------------------------------------------------*
     *  Lock
     *--------------------------------------------------*/
    uv_mutex_lock(&mutex_gbmem);

    /*---------------------------------*
     *  Recover size (in blocks)
     *---------------------------------*/
    p = (bdl_t *)p - 1;
    blocks = *((bdl_t *)p);

    /*---------------------------------*
     *  Recover trace
     *---------------------------------*/
    #ifdef __FULL_TRACE__
    p = (memtrace_t *)p - 1;
    memtrace_t x = *((memtrace_t *)p);
    #endif

    if(blocks > pool_size) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "blocks > pool_size",
            "pointer",      "%p", p,
            "blocks",       "%d", (int)blocks,
            "pool_size",    "%d", (int)pool_size,
            NULL
        );
    } else if(blocks <= 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "BLOCK ALREADY DELETED?",
            "pointer",      "%p", p,
            "blocks",       "%d", (int)blocks,
            NULL);
        abort();
    } else {
        --blocks;
        ((ZBLOCK *)p)->link = pool[blocks];
        pool[blocks] = (ZBLOCK *)p;
        pool_sizes[blocks]++;
    }

    #ifdef __FULL_TRACE__
    if(__trace_allocs_frees__) {
        log_debug(0,
            "gobj",         "%s", __FILE__,
            "msgset",       "%s", MSGSET_STATISTICS,
            "msg",          "%s", "free memory",
            "memtrace",     "%d", (int)x,
            "blocks",       "%d", (int)blocks+1,
            "pointer",      "%p", pp,
            NULL);
    }
    #endif

    /*--------------------------------------------------*
     *  Unlock
     *--------------------------------------------------*/
    uv_mutex_unlock(&mutex_gbmem);
}

/***************************************************************************
 *     ReAlloca memoria del core
 ***************************************************************************/
PUBLIC PTR gbmem_realloc(PTR p, size_t new_size)
{
    register PTR q;
    PTR pp;
    size_t old_size;
    bdl_t blocks;

    /*---------------------------------*
     *  realloc admit p null
     *---------------------------------*/
    if(!p) {
        return gbmem_malloc(new_size);
    }

    if(!__gbmem_initialized__) { // XXX
//         return realloc(p, new_size);

        new_size += EXTRA_MEM + sizeof(size_t);

        char *pm = p;
        pm -= sizeof(size_t);

        size_t *pm_ = (size_t*)pm;
        size_t size = *pm_;

        __cur_system_memory__ -= size;

        __cur_system_memory__ += new_size;
        if(__cur_system_memory__ > __max_system_memory__) {
            log_critical(0,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "process",              "%s", get_process_name(),
                "hostname",             "%s", get_host_name(),
                "pid",                  "%d", get_pid(),
                "msgset",               "%s", MSGSET_MEMORY_ERROR,
                "msg",                  "%s", "REACHED MAX_SYSTEM_MEMORY",
                "cur_system_memory",    "%ld", (long)__cur_system_memory__,
                "max_system_memory",    "%ld", (long)__max_system_memory__,
                NULL
            );
            abort();
        }
        char *pm__ = realloc(pm, new_size);
        if(!pm__) {
            log_critical(0,
                "gobj",                 "%s", __FILE__,
                "function",             "%s", __FUNCTION__,
                "process",              "%s", get_process_name(),
                "hostname",             "%s", get_host_name(),
                "pid",                  "%d", get_pid(),
                "msgset",               "%s", MSGSET_MEMORY_ERROR,
                "msg",                  "%s", "NO MEMORY realloc() failed",
                "new_size",             "%ld", (long)new_size,
                NULL
            );
            abort();
        }
        pm = pm__;
        pm_ = (size_t*)pm;
        *pm_ = new_size;
        pm += sizeof(size_t);
        return pm;
    }

    /*---------------------------------*
     *  Recover size (in blocks)
     *---------------------------------*/
    pp = (bdl_t *)p - 1;
    blocks = *((bdl_t *)pp);
    old_size = (size_t) BlocksToBytes(blocks);

    q = gbmem_malloc(new_size);
    if(!q) {
        return (PTR)0;
    }
    memcpy(q, p, old_size < new_size ? old_size : new_size);
    gbmem_free(p);

    return q;
}

/***************************************************************************
 *     calloc with gbmem
 ***************************************************************************/
PUBLIC PTR gbmem_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    return gbmem_malloc(total);
}

/***************************************************************************
 *     Duplica un string
 ***************************************************************************/
PUBLIC char * gbmem_strdup(const char *string)
{
    if(!string) {
        return NULL;
    }
    return gbmem_substr_dup(string, strlen(string));
}

/***************************************************************************
 *     Duplica un string
 ***************************************************************************/
PUBLIC char * gbmem_strndup(const char *string, size_t n)
{
    if(!string) {
        return NULL;
    }
    return gbmem_substr_dup(string, n);
}

/***************************************************************************
 *     duplicate a substring
 ***************************************************************************/
PRIVATE char * gbmem_substr_dup(const char *str, size_t size)
{
    char *s;
    int len;

    /*-----------------------------------------*
     *     Check null string
     *-----------------------------------------*/
    if(!str) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "str is NULL",
            NULL);
        return NULL;
    }

    /*-----------------------------------------*
     *     Alloca memoria
     *-----------------------------------------*/
    len = size;
    s = (char *)gbmem_malloc(len+1);
    if(!s) {
        return NULL;
    }

    /*-----------------------------------------*
     *     Copia el substring
     *-----------------------------------------*/
    strncpy(s, str, len);

    return s;
}

/*************************************************************************
 *  Return the maximum memory that you can get
 *************************************************************************/
PUBLIC size_t gbmem_get_maximum_block(void)
{
    if(!__gbmem_initialized__) {
        return __max_block__;
    }

    size_t size = __max_block__;
    size -= sizeof(bdl_t);
    size -= 1;
    #ifdef __FULL_TRACE__
    size -= sizeof(memtrace_t);
    #endif
    size -= __min_block__;  // added 7/Sep/2016
    return size;
}

/*************************************************************************
 *  Set trace of alloc/free
 *  You need to compile with ghelpersd (trace version)
 *************************************************************************/
PUBLIC void gbmem_trace_alloc_free(
    BOOL enable,
    uint32_t *memory_check_list)
{
    __trace_allocs_frees__ = enable;
    __memory_check_list__ = memory_check_list;
}

/*************************************************************************
 *   memory stats
 *************************************************************************/
PUBLIC void gbmem_stats(
    size_t *free_superblock_mem_,
    size_t *free_segmented_mem_,
    size_t *total_free_mem_,
    size_t *allocated_system_mem_,
    size_t *mem_in_use_,
    size_t *cur_sm_,
    size_t *max_sm_)
{
    size_t free_superblock_mem=0;
    size_t free_segmented_mem=0;
    size_t total_free_mem=0;
    size_t allocated_system_mem=0;
    size_t mem_in_use=0;

    if(__gbmem_initialized__) {
        unsigned i, j;
        ZBLOCK *pz;

        /*-----------------------------------------------*
        *   Bloques
        *-----------------------------------------------*/
        for(i=0; i<pool_size; i++) {
            pz = pool[i];
            if(!pz) {
                if(pool_sizes[i] != 0) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "process",      "%s", get_process_name(),
                        "hostname",     "%s", get_host_name(),
                        "pid",          "%d", get_pid(),
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "incongruent pool_sizes",
                        "blocks",       "%d", (int)i+1,
                        "have",         "%d", (int)pool_sizes[i],
                        "must",         "%d", (int)0,
                        NULL);
                }
                continue;
            }
            j = 0;
            while(pz) {
                j++;
                free_segmented_mem += (i+1)*(block_size);
                pz = pz->link;
            }
            if(pool_sizes[i] != j) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "process",      "%s", get_process_name(),
                    "hostname",     "%s", get_host_name(),
                    "pid",          "%d", get_pid(),
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "incongruent pool_sizes",
                    "blocks",       "%d", (int)i+1,
                    "have",         "%d", (int)pool_sizes[i],
                    "must",         "%d", (int)j,
                    NULL);
            }
        }

        free_superblock_mem = (size_t)(amt_avail) * (size_t)block_size;
        total_free_mem = free_segmented_mem + free_superblock_mem;
        allocated_system_mem = (size_t)count_sb * (size_t)chunk * (size_t)block_size;
        mem_in_use = allocated_system_mem - total_free_mem;

        if(free_superblock_mem_) {
            *free_superblock_mem_ = (size_t)free_superblock_mem;
        }
        if(free_segmented_mem_) {
            *free_segmented_mem_ = (size_t)free_segmented_mem;
        }
        if(total_free_mem_) {
            *total_free_mem_ = (size_t)total_free_mem;
        }
        if(allocated_system_mem_) {
            *allocated_system_mem_ = (size_t)allocated_system_mem;
        }
        if(mem_in_use_) {
            *mem_in_use_ = (size_t)mem_in_use;
        }
        if(cur_sm_) {
            *cur_sm_ = (size_t)allocated_system_mem;
        }
        if(max_sm_) {
            *max_sm_ = (size_t)__max_system_memory__;
        }
    } else {
        if(free_superblock_mem_) {
            *free_superblock_mem_ = (size_t)free_superblock_mem;
        }
        if(free_segmented_mem_) {
            *free_segmented_mem_ = (size_t)free_segmented_mem;
        }
        if(total_free_mem_) {
            *total_free_mem_ = (size_t)total_free_mem;
        }
        if(allocated_system_mem_) {
            *allocated_system_mem_ = (size_t)allocated_system_mem;
        }
        if(mem_in_use_) {
            *mem_in_use_ = (size_t)__cur_system_memory__;
        }

        if(cur_sm_) {
            *cur_sm_ = (size_t)__cur_system_memory__;
        }
        if(max_sm_) {
            *max_sm_ = (size_t)__max_system_memory__;
        }
    }
}

/*************************************************************************
 *   Info memory stats
 *************************************************************************/
PUBLIC size_t gbmem_mem_in_use(void)
{
    size_t mem_in_use;
    gbmem_stats(
        0,
        0,
        0,
        0,
        &mem_in_use,
        0,
        0
    );
    return mem_in_use;
}

/*************************************************************************
 *   Info memory stats
 *************************************************************************/
PUBLIC void gbmem_log_info(BOOL final)
{
    size_t free_superblock_mem=0;
    size_t free_segmented_mem=0;
    size_t total_free_mem=0;
    size_t allocated_system_mem=0;
    size_t mem_in_use=0;
    size_t cur_sb;
    size_t max_sb;

    gbmem_stats(
        &free_superblock_mem,
        &free_segmented_mem,
        &total_free_mem,
        &allocated_system_mem,
        &mem_in_use,
        &cur_sb,
        &max_sb
    );
    log_debug(0,
        "gobj",                 "%s", __FILE__,
        "final-count",          "%s", final?"true":"false",
        "process",              "%s", get_process_name(),
        "hostname",             "%s", get_host_name(),
        "pid",                  "%d", get_pid(),
        "msgset",               "%s", MSGSET_STATISTICS,
        "msg",                  "%s", "memory stats",

        "MIN_BLOCK",            "%ld", (long)__min_block__,
        "MAX_BLOCK",            "%ld", (long)__max_block__,
        "SUPERBLOCK",           "%ld", (long)__superblock__,
        "MAX_SYSTEM_MEMORY",    "%ld", (long)__max_system_memory__,

        "block_size",           "%ld", (long)block_size,
        "pool_size",            "%ld", (long)pool_size,
        "chunk",                "%ld", (long)(size_t)chunk,
        "count_sb",             "%ld", (long)count_sb,
        "maxsb",                "%ld", (long)maxsb,
        "free_superblock_mem",  "%ld", (long)free_superblock_mem,
        "free_segmented_mem",   "%ld", (long)free_segmented_mem,
        "total_free_mem",       "%ld", (long)total_free_mem,
        "mem_in_use",           "%ld", (long)mem_in_use,
        "allocated_system_mem", "%ld", (long)allocated_system_mem,
        NULL
    );
    if(final && mem_in_use) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_STATISTICS,
            "msg",          "%s", "shutdown: gbmem memory not free",
            "mem_in_use",   "%ld", (long)mem_in_use,
            NULL
        );
    }
}

/*************************************************************************
 *   Return json with mem stats
 *************************************************************************/
PUBLIC void *gbmem_json_info(BOOL display_free_segmented_mem)
{
    size_t free_superblock_mem=0;
    size_t free_segmented_mem=0;
    size_t total_free_mem=0;
    size_t allocated_system_mem=0;
    size_t mem_in_use=0;
    size_t cur_sb;
    size_t max_sb;

    gbmem_stats(
        &free_superblock_mem,
        &free_segmented_mem,
        &total_free_mem,
        &allocated_system_mem,
        &mem_in_use,
        &cur_sb,
        &max_sb
    );

    json_t *jn_m = json_object();

    json_object_set_new(jn_m, "MIN_BLOCK", json_integer(__min_block__));
    json_object_set_new(jn_m, "MAX_BLOCK", json_integer(__max_block__));
    json_object_set_new(jn_m, "SUPERBLOCK", json_integer(__superblock__));
    json_object_set_new(jn_m, "MAX_SYSTEM_MEMORY", json_integer(__max_system_memory__));

    json_object_set_new(jn_m, "block_size", json_integer(block_size));
    json_object_set_new(jn_m, "pool_size", json_integer(pool_size));
    json_object_set_new(jn_m, "chunk", json_integer(chunk));
    json_object_set_new(jn_m, "count_sb", json_integer(count_sb));
    json_object_set_new(jn_m, "maxsb", json_integer(maxsb));
    json_object_set_new(jn_m, "free_superblock_mem", json_integer(free_superblock_mem));
    json_object_set_new(jn_m, "free_segmented_mem", json_integer(free_segmented_mem));
    json_object_set_new(jn_m, "total_free_mem", json_integer(total_free_mem));
    json_object_set_new(jn_m, "mem_in_use", json_integer(mem_in_use));
    json_object_set_new(jn_m, "allocated_system_mem", json_integer(allocated_system_mem));

    if(display_free_segmented_mem) {
        json_t *jn_free_segmented_mem = json_object();
        for(int i=0; i<pool_size; i++) {
            char temp[20];
            if(pool_sizes[i] > 0) {
                snprintf(temp, sizeof(temp), "%d", (int)i+1);
                json_object_set_new(
                    jn_free_segmented_mem,
                    temp,
                    json_integer(pool_sizes[i])
                );
            }
        }
        json_object_set_new(jn_m, "free_segmented_mem", jn_free_segmented_mem);
    }

    return jn_m;
}

/*************************************************************************
 *   Enable log info of memory stats
 *************************************************************************/
PUBLIC void gbmem_enable_log_info(BOOL enable)
{
    __log_info_enabled__ = enable?1:0;
}
