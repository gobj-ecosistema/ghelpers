/***********************************************************************
 *              GBUFFER.C
 *              Buffer growable and overflowable
 *              Copyright (c) 2013-2016 Niyamaka.
 *              All Rights Reserved.
 ***********************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include "20_gbuffer.h"

/****************************************************************
 *         Constants
 ****************************************************************/
/*
 * fseeko64, ftello64, tmpfile64 not recognize in _WIN32
 */

/****************************************************************
 *         Structures
 ****************************************************************/

/****************************************************************
 *         Data
 ****************************************************************/
PRIVATE BOOL __trace_create_delete__ = 0;

/****************************************************************
 *         Prototypes
 ****************************************************************/

/***************************************************************************
 *  Crea un gbuf de tamaño de datos 'data_size'
 *  Retorna NULL if error
 ***************************************************************************/
PUBLIC GBUFFER * gbuf_create(
    size_t data_size,
    size_t max_memory_size,
    size_t max_disk_size,
    gbuf_encoding encoding)
{
    GBUFFER *gbuf;

    /*---------------------------------*
     *   Alloc memory
     *---------------------------------*/
    gbuf = gbmem_malloc(sizeof(struct _GBUFFER));
    if(!gbuf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "bmem_malloc() return NULL",
            "sizeof",       "%d", sizeof(struct _GBUFFER),
            NULL
        );
        return (GBUFFER *)0;
    }

    gbuf->data = gbmem_malloc(data_size+1);
    if(!gbuf->data) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "bmem_malloc() return NULL",
            "data_size",    "%d", data_size,
            NULL
        );
        gbmem_free(gbuf);
        return (GBUFFER *)0;
    }

    /*---------------------------------*
     *   Inicializa atributos
     *---------------------------------*/
    gbuf->data_size = data_size;
    gbuf->max_memory_size = max_memory_size;
    gbuf->max_disk_size = max_disk_size;
    gbuf->encoding = encoding;

    gbuf->tail = 0;
    gbuf->curp = 0;
    gbuf->refcount = 1;

    /*----------------------------*
     *   Retorna pointer a gbuf
     *----------------------------*/
    if(__trace_create_delete__) {
        log_debug(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_CREATION_DELETION_GBUFFERS,
            "msg",          "%s", "Creating gbuffer",
            "pointer",      "%p", gbuf,
            "data_size",    "%d", data_size,
            NULL);
    }

    return gbuf;
}

/***************************************************************************
 *    Realloc buffer
 ***************************************************************************/
PRIVATE inline void _set_writting(GBUFFER *gbuf, BOOL set)
{
    if(gbuf->writting && set) {
        // Already writting;
    } else if(!gbuf->writting && set) {
        // Set writting
        fseeko64(gbuf->tmpfile, 0, SEEK_END);
        gbuf->writting = TRUE;
    } else if(gbuf->writting && !set) {
        // Set reading
        fseeko64(gbuf->tmpfile, gbuf->curp, SEEK_SET);
        gbuf->writting = FALSE;
    } else { // !gbuf->writting && !set
        // Already reading
    }

    if(gbuf->writting) {
        size_t write_pos = ftello64(gbuf->tmpfile);
        if(write_pos != gbuf->tail) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "INVALID gbuf->tail",
                "write_pos",    "%ld", write_pos,
                "tail",         "%ld", gbuf->tail,
                NULL
            );
        }
    } else {
        size_t read_pos = ftello64(gbuf->tmpfile);
        if(read_pos != gbuf->curp) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "INVALID gbuf->curp",
                "read_pos",     "%ld", read_pos,
                "curp",         "%ld", gbuf->curp,
                NULL
            );
        }
    }
}

/***************************************************************************
 *    Realloc buffer
 ***************************************************************************/
PRIVATE BOOL _gbuf_realloc(GBUFFER *gbuf, size_t need_size)
{
    size_t more;
    char *new_buf;

    if(gbuf->file_mode) {
        return TRUE;
    }

    more = gbuf->data_size + MAX(gbuf->data_size, need_size);
    if(more > gbuf->max_memory_size) {
        if(gbuf->max_disk_size < gbuf->max_memory_size || more > gbuf->max_disk_size) {
            /*
             *  No option to create temp file
             */
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",             "%s", __FILE__,
                "function",         "%s", __FUNCTION__,
                "process",          "%s", get_process_name(),
                "hostname",         "%s", get_host_name(),
                "pid",              "%d", get_pid(),
                "msgset",           "%s", MSGSET_INTERNAL_ERROR,
                "msg",              "%s", "MAXIMUM SPACE REACHED",
                "more",             "%ld", more,
                "max_disk_size",    "%ld", gbuf->max_disk_size,
                "max_memory_size",  "%ld", gbuf->max_memory_size,
                NULL
            );
            return FALSE;
        }
        /*
         *  Create temporary file.
         */
        if(!gbuf->tmpfile) {
            gbuf->tmpfile = tmpfile64();
            if(!gbuf->tmpfile) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",             "%s", __FILE__,
                    "function",         "%s", __FUNCTION__,
                    "process",          "%s", get_process_name(),
                    "hostname",         "%s", get_host_name(),
                    "pid",              "%d", get_pid(),
                    "msgset",           "%s", MSGSET_INTERNAL_ERROR,
                    "msg",              "%s", "tmpfile() FAILED",
                    "more",             "%ld", more,
                    "max_disk_size",    "%ld", gbuf->max_disk_size,
                    "max_memory_size",  "%ld", gbuf->max_memory_size,
                    "errno",            "%d", errno,
                    "strerror",         "%s", strerror(errno),
                    NULL
                );
                return FALSE;
            }
        }
        /*
         *  Enable file_mode, pass all data to file
         */
        if(gbuf->tail) {
            size_t written = fwrite(gbuf->data, gbuf->tail, 1, gbuf->tmpfile);
            if(!written) {
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "process",      "%s", get_process_name(),
                    "hostname",     "%s", get_host_name(),
                    "pid",          "%d", get_pid(),
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "fwrite() FAILED",
                    "errno",        "%d", errno,
                    "strerror",     "%s", strerror(errno),
                    NULL
                );
                return FALSE;
            }
            fflush(gbuf->tmpfile);
        }
        gbuf->file_mode = TRUE;
        _set_writting(gbuf, TRUE);
        if(__trace_create_delete__) {
            log_debug(0,
                "gobj",             "%s", __FILE__,
                "function",         "%s", __FUNCTION__,
                "process",          "%s", get_process_name(),
                "hostname",         "%s", get_host_name(),
                "pid",              "%d", get_pid(),
                "msgset",           "%s", MSGSET_CREATION_DELETION_GBUFFERS,
                "msg",              "%s", "Reallocationg gbuffer to DISK",
                "pointer",          "%p", gbuf,
                "more",             "%ld", more,
                "max_disk_size",    "%ld", gbuf->max_disk_size,
                "max_memory_size",  "%ld", gbuf->max_memory_size,
                NULL
            );
        }
        return TRUE;
    }

    /*
     *  Realloc buffer
     */
    new_buf = gbmem_realloc(gbuf->data, more+1);
    if(!new_buf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NOT ENOUGH SPACE",
            "more",         "%d", more,
            NULL
        );
        return FALSE;
    }
    gbuf->data = new_buf;
    gbuf->data_size = more;
    if(__trace_create_delete__) {
        log_debug(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_CREATION_DELETION_GBUFFERS,
            "msg",          "%s", "Reallocationg gbuffer",
            "pointer",      "%p", gbuf,
            "more",         "%d", more,
            NULL
        );
    }
    return TRUE;
}

/***************************************************************************
 *    Elimina paquete
 ***************************************************************************/
PUBLIC void gbuf_remove(GBUFFER *gbuf)
{
    if(__trace_create_delete__) {
        log_debug(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_CREATION_DELETION_GBUFFERS,
            "msg",          "%s", "Deleting gbuffer",
            "pointer",      "%p", gbuf,
            NULL);
    }

    /*-----------------------*
     *  Callback on destroy
     *-----------------------*/
    if(gbuf->on_destroy_gbuf) {
        (*gbuf->on_destroy_gbuf) (
            gbuf->param1,
            gbuf->param2,
            gbuf->param3,
            gbuf->param4
        );
    }

    /*-----------------------*
     *  Libera la memoria
     *-----------------------*/
    if(gbuf->label) {
        gbmem_free((char *)gbuf->label);
        gbuf->label = 0;
    }
    if(gbuf->data) {
        gbmem_free(gbuf->data);
        gbuf->data = 0;
    }
    if(gbuf->tmpfile) {
        fclose(gbuf->tmpfile);
        gbuf->tmpfile = 0;
    }

    gbmem_free(gbuf);
}

/***************************************************************************
 *    Incr ref
 ***************************************************************************/
PUBLIC void gbuf_incref(GBUFFER *gbuf)
{
    if(!gbuf || gbuf->refcount <= 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "BAD gbuf_incref()",
            NULL
        );
        return;
    }
    ++(gbuf->refcount);
}

/***************************************************************************
 *    Decr ref
 ***************************************************************************/
PUBLIC void gbuf_decref(GBUFFER *gbuf)
{
    if(!gbuf || gbuf->refcount <= 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "BAD gbuf_decref()",
            NULL
        );
        return;
    }
    --(gbuf->refcount);
    if(gbuf->refcount == 0)
        gbuf_remove(gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gbuf_set_on_destroy_gbuf(
    GBUFFER *gbuf,
    int (*on_destroy_gbuf)(void *param1, void *param2, void *param3, void *param4),
    void *param1,
    void *param2,
    void *param3,
    void *param4
)
{
    gbuf->on_destroy_gbuf = on_destroy_gbuf;
    gbuf->param1 = param1;
    gbuf->param2 = param2;
    gbuf->param3 = param3;
    gbuf->param4 = param4;
}

/***************************************************************************
 *    Devuelve puntero a la actual posicion de salida de datos
 ***************************************************************************/
PUBLIC void * gbuf_cur_rd_pointer(GBUFFER *gbuf)
{
    char *p;

    if(!gbuf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return 0;
    }
    p = gbuf->data;
    p += gbuf->curp;
    return p;
}

/***************************************************************************
 *    Devuelve puntero a la actual posicion de entrada de datos
 ***************************************************************************/
PRIVATE inline void * _gbuf_cur_wr_pointer(GBUFFER *gbuf)
{
    return gbuf->data + gbuf->tail;
}




                /***************************
                 *      READING
                 ***************************/




/***************************************************************************
 *    Reset current output pointer
 ***************************************************************************/
PUBLIC void gbuf_reset_rd(GBUFFER *gbuf)
{
    gbuf->curp = 0;

    if(gbuf->file_mode) {
        gbuf->writting = TRUE; // to force reading
        _set_writting(gbuf, FALSE);
    }
}

/***************************************************************************
 *  Set current output pointer
 ***************************************************************************/
PUBLIC int gbuf_set_rd_offset(GBUFFER *gbuf, size_t position)
{
    if(position >= gbuf->data_size) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "len GREATER than data_size",
            "len",          "%d", position,
            "data_size",    "%d", gbuf->data_size,
            NULL
        );
        return -1;
    }
    if(position > gbuf->tail) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "len GREATER than curp",
            "len",          "%d", position,
            "curp",         "%d", gbuf->curp,
            NULL
        );
        return -1;
    }
    gbuf->curp = position;
    return 0;
}

/***************************************************************************
 *  Unget char
 ***************************************************************************/
PUBLIC int gbuf_ungetc(GBUFFER* gbuf, char c)
{
    if(gbuf->curp > 0) {
        gbuf->curp--;

        *(gbuf->data + gbuf->curp) = c;
    }

    return 0;
}

/***************************************************************************
 *  Get current output pointer
 ***************************************************************************/
PUBLIC size_t gbuf_get_rd_offset(GBUFFER* gbuf)
{
    return gbuf->curp;
}

/***************************************************************************
 *    Saca 'len' bytes del gbuf.
 *    Devuelve al pointer al primer byte de los 'len' datos sacados.
 *    WARNING primero hay que asegurarse que existen 'len' bytes disponibles
 ***************************************************************************/
PUBLIC void * gbuf_get(GBUFFER *gbuf, size_t len)
{
    BOOL skip=TRUE; // TODO enable get without pop the data

    if(!gbuf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return 0;
    }

    if(len <= 0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "len is <= 0",
            "len",          "%d", (int)len,
            NULL
        );
        return 0;
    }

    /*--------------------------*
     *  Using data in memory
     *--------------------------*/
    if(!gbuf->file_mode) {
        size_t rest = gbuf_leftbytes(gbuf);

        if(len > rest) {
            return 0;
        }
        char *p;
        p = gbuf->data;
        p += gbuf->curp;
        if(skip) {
            gbuf->curp += len;     /* elimina los bytes del gbuf */
        }
        return p;
    }

    /*--------------------------*
     *  Using data in file
     *--------------------------*/
    _set_writting(gbuf, FALSE);
    if(len > gbuf->data_size) {
        /*
         *  You must read in data_size chunks!
         */
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuf: in file mode you MUST READ in data_size CHUNKS",
            "len",          "%d", (int)len,
            "data_size",    "%d", (int)gbuf->data_size,
            NULL
        );
        return 0;
    }

    size_t readed = fread(gbuf->data, len,  1, gbuf->tmpfile);
    if(readed != 1) {
        return 0;
    }

    if(skip) {
        gbuf->curp += len;     /* elimina los bytes del gbuf */
    } else {
        // set at curp again
        fseeko64(gbuf->tmpfile, gbuf->curp, SEEK_SET);
    }
    return gbuf->data;
}

/***************************************************************************
 *  pop one byte
 ***************************************************************************/
PUBLIC char gbuf_getchar(GBUFFER *gbuf)
{
    char *p = gbuf_get(gbuf, 1);
    if(p)
        return *p;
    else
        return 0;
}

/***************************************************************************
 *  return the chunk of data available
 ***************************************************************************/
PUBLIC size_t gbuf_chunk(GBUFFER *gbuf)
{
    int ln = gbuf_leftbytes(gbuf);
    int chunk_size = MIN(gbuf->data_size, ln);

    return chunk_size;
}


/***************************************************************************
 *
 ***************************************************************************/
void *
memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
    register char *cur, *last;
    const char *cl = (const char *)l;
    const char *cs = (const char *)s;

    /* we need something to compare */
    if (l_len == 0 || s_len == 0)
        return NULL;

    /* "s" must be smaller or equal to "l" */
    if (l_len < s_len)
        return NULL;

    /* special case where s_len == 1 */
    if (s_len == 1)
        return memchr(l, (int)*cs, l_len);

    /* the last position where its possible to find "s" in "l" */
    last = (char *)cl + l_len - s_len;

    for (cur = (char *)cl; cur <= last; cur++)
        if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
            return cur;

    return NULL;
}

/***************************************************************************
 *  Put read pointer in found pattern
 ***************************************************************************/
PUBLIC void *gbuf_find(GBUFFER *gbuf, const char *pattern, int patter_len)
{
    /*-------------------------------------------------------------*
     *  Using data in memory
     *  Copied from FreeBSD memmem.c
     *  Copyright (c) 2005 Pascal Gloor <pascal.gloor@spale.com>
     *-------------------------------------------------------------*/
    if(!gbuf->file_mode) {
        /*
         * Find the first occurrence of the byte string cs in byte string cl.
         */
        register char *cur, *last;
        const char *p = (char *)gbuf->data + gbuf->curp;

        size_t resto = gbuf_leftbytes(gbuf);

        /* we need something to compare */
        if (resto == 0 || patter_len == 0)
            return NULL;

        /* "s" must be smaller or equal to "l" */
        if (resto < patter_len)
            return NULL;

        /* special case where len == 1 */
        if (patter_len == 1)
            return memchr(p, (int)*pattern, resto);

        /* the last position where its possible to find "s" in "l" */
        last = (char *)p + resto - patter_len;

        for (cur = (char *)p; cur <= last; cur++) {
            if (cur[0] == pattern[0] && memcmp(cur, pattern, patter_len) == 0) {
                gbuf->curp = cur - gbuf->data;
                return cur;
            }
        }

        return 0;
    }

    /*--------------------------*
     *  Using data in file
     *--------------------------*/
    //TODO
    log_error(LOG_OPT_TRACE_STACK,
        "gobj",         "%s", __FILE__,
        "function",     "%s", __FUNCTION__,
        "process",      "%s", get_process_name(),
        "hostname",     "%s", get_host_name(),
        "pid",          "%d", get_pid(),
        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
        "msg",          "%s", "find in file mode NOT IMPLEMENTED",
        NULL
    );
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *gbuf_getline(GBUFFER *gbuf, char separator)
{
    if(gbuf_leftbytes(gbuf)<=0) {
        // No more chars
        return (char *)0;
    }
    char *begin = gbuf_cur_rd_pointer(gbuf); // TODO WARNING with chunks this will failed!
    register char *p;
    while((p=gbuf_get(gbuf,1))) {
        if(*p == separator) {
            *p = 0;
            return begin;
        }
    }

    return begin;
}




                /***************************
                 *      WRITTING
                 ***************************/



/***************************************************************************
 *    Get current writing position
 ***************************************************************************/
PUBLIC void *gbuf_cur_wr_pointer(GBUFFER *gbuf)
{
    return gbuf->data + gbuf->tail;
}

/***************************************************************************
 *    Reset current input pointer
 ***************************************************************************/
PUBLIC void gbuf_reset_wr(GBUFFER *gbuf)
{
    gbuf->tail = 0;
    gbuf->curp = 0;

    /*
     *  Put final null
     */
    char *p = gbuf->data;
    p += gbuf->tail;
    *p = 0;

    if(gbuf->file_mode) {
        if(gbuf->tmpfile) {
            fclose(gbuf->tmpfile);
            gbuf->tmpfile = 0;
        }
        gbuf->tmpfile = tmpfile64();
        if(!gbuf->tmpfile) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "tmpfile() FAILED",
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            return;
        }
        gbuf->writting = FALSE; // to force writting
        _set_writting(gbuf, TRUE);
    }
}

/***************************************************************************
 *  Set current input pointer
 *  Useful when using gbuf as write buffer
 ***************************************************************************/
PUBLIC int gbuf_set_wr(GBUFFER *gbuf, size_t offset)
{
    if(offset > gbuf->data_size) { // WARNING collateral damage? (original>=), version 3.4.3
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "offset GREATER than data_size",
            "offset",       "%d", offset,
            "data_size",    "%d", gbuf->data_size,
            NULL
        );
        return -1;
    }
    gbuf->tail = offset;

    /*
     *  Put final null
     */
    char *p = gbuf->data;
    p += gbuf->tail;
    *p = 0;

    return 0;
}

/***************************************************************************
 *   Append 'len' bytes
 *   Retorna bytes guardados
 ***************************************************************************/
PUBLIC size_t gbuf_append(GBUFFER *gbuf, void *bf, size_t len)
{
    char *p;

    if(!bf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "bf is NULL",
            NULL
        );
        return 0;
    }
    if(!len) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "len is ZERO",
            NULL
        );
        return 0;
    }
    if(gbuf_freebytes(gbuf) < len) {
        _gbuf_realloc(gbuf, len);
    }
    if(gbuf_freebytes(gbuf) < len) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NOT ENOUGH SPACE, append only 'free' bytes",
            "free",         "%d", gbuf_freebytes(gbuf),
            "needed",       "%d", len,
            NULL
        );
        len = gbuf_freebytes(gbuf);
        if(len==0) {
            return 0;
        }
    }

    /*--------------------------*
     *  Using data in memory
     *--------------------------*/
    if(!gbuf->file_mode) {
        p = gbuf->data;
        memmove(p + gbuf->tail, bf, len);
        gbuf->tail += len;
        *(p + gbuf->tail) = 0;
        return len;
    }

    /*--------------------------*
     *  Using data in file
     *--------------------------*/
    _set_writting(gbuf, TRUE);
    if(!fwrite(bf, len, 1, gbuf->tmpfile)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "fwrite() FAILED",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return 0;
    }
    gbuf->tail += len;
    return len;
}

/***************************************************************************
 *   Append 'len' bytes
 *   Retorna bytes guardados
 ***************************************************************************/
PUBLIC size_t gbuf_append_string(GBUFFER *gbuf, const char *s)
{
    return gbuf_append(gbuf, (void *)s, strlen(s));
}

/***************************************************************************
 *   Append 'c'
 *   Retorna bytes guardados
 ***************************************************************************/
PUBLIC size_t gbuf_append_char(GBUFFER *gbuf, char c)
{
    return gbuf_append(gbuf, &c, 1);
}

/***************************************************************************
 *   Append a gbuf to another gbuf. The two gbuf must exist.
 ***************************************************************************/
PUBLIC int gbuf_append_gbuf(GBUFFER *dst, GBUFFER *src)
{
    register char *p;
    int ln = gbuf_leftbytes(src);
    int chunk_size = MIN(src->data_size, ln);

    while(ln>0) {
        p = gbuf_get(src, chunk_size);
        if(!p) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gbuf_get() FAILED",
                NULL
            );
            return -1;
        }
        if(gbuf_append(dst, p, chunk_size)!=chunk_size) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gbuf_append() FAILED",
                NULL
            );
            return -1;
        }
        ln = gbuf_leftbytes(src);
        chunk_size = MIN(src->data_size, ln);
    }
    return 0;
}

/***************************************************************************
 *    ESCRITURA del mensaje.
 *    Añade message with format al final del mensaje
 *    Return bytes written
 ***************************************************************************/
PUBLIC int gbuf_printf(GBUFFER *gbuf, const char *format, ...)
{
    BOOL ret;
    va_list ap;

    va_start(ap, format);
    ret = gbuf_vprintf(gbuf, format, ap);
    va_end(ap);
    return ret;
}

/***************************************************************************
 *    ESCRITURA del mensaje.
 *    Añade message with format al final del mensaje
 *    Return bytes written
 ***************************************************************************/
PUBLIC int gbuf_vprintf(GBUFFER *gbuf, const char *format, va_list ap)
{
    char *bf;
    size_t len;
    int written;

    va_list aq;

    /*--------------------------*
     *  Using data in memory
     *--------------------------*/
    if(!gbuf->file_mode) {
        bf = _gbuf_cur_wr_pointer(gbuf);
        len = gbuf_freebytes(gbuf);

        va_copy(aq, ap);
        written = vsnprintf(bf, len, format, aq);
        va_end(aq);
        if(written < 0) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "vsnprintf FAILED",
                NULL
            );
            written = 0;

        } else if(written >= len) {
            if(!_gbuf_realloc(gbuf, written)) {
                written = 0;
            } else {
                bf = _gbuf_cur_wr_pointer(gbuf);
                len = gbuf_freebytes(gbuf);
                va_copy(aq, ap);
                written = vsnprintf(bf, len, format, aq);
                va_end(aq);
                if(written < 0) {
                    log_error(LOG_OPT_TRACE_STACK,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "process",      "%s", get_process_name(),
                        "hostname",     "%s", get_host_name(),
                        "pid",          "%d", get_pid(),
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "vsnprintf FAILED",
                        "errno",        "%d", errno,
                        "strerror",     "%s", strerror(errno),
                        NULL
                    );
                    written = 0;
                } else if(written >= len) {
                    log_error(LOG_OPT_TRACE_STACK,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "process",      "%s", get_process_name(),
                        "hostname",     "%s", get_host_name(),
                        "pid",          "%d", get_pid(),
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "NOT ENOUGH SPACE",
                        "free",         "%d", gbuf_freebytes(gbuf),
                        "needed",       "%d", len,
                        NULL
                    );
                    gbuf->tail += len;
                } else {
                    gbuf->tail += written;
                }
            }
        } else {
            gbuf->tail += written;
        }
        return written;
    }

    /*--------------------------*
     *  Using data in file
     *--------------------------*/
    _set_writting(gbuf, TRUE);

    va_copy(aq, ap);
    written = vfprintf(gbuf->tmpfile, format, ap);
    va_end(aq);
    if(written > 0) {
        gbuf->tail += written;
    } else {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "vfprintf FAILED",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    }
    return written;
}




                /***************************
                 *      Util
                 ***************************/




/***************************************************************************
 *  Return pointer to first position of data
 ***************************************************************************/
PUBLIC void *gbuf_head_pointer(GBUFFER *gbuf)
{
    char *p;

    if(!gbuf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return 0;
    }
    p = gbuf->data;
    return p;
}

/***************************************************************************
 *  Reset write/read pointers
 ***************************************************************************/
PUBLIC void gbuf_clear(GBUFFER *gbuf)
{
    if(!gbuf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return;
    }
    gbuf_reset_wr(gbuf);
    gbuf_reset_rd(gbuf);
}

/***************************************************************************
 *    Devuelve los bytes que quedan en el packet por procesar
 ***************************************************************************/
PUBLIC size_t gbuf_leftbytes(GBUFFER *gbuf)
{
    if(!gbuf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return 0;
    }
    return gbuf->tail - gbuf->curp;
}

/***************************************************************************
 *    Devuelve el número de bytes escritos
 ***************************************************************************/
PUBLIC size_t gbuf_totalbytes(GBUFFER *gbuf)
{
    return gbuf->tail;
}

/***************************************************************************
 *    Devuelve el espacio libre de bytes de datos
 ***************************************************************************/
PUBLIC size_t gbuf_freebytes(GBUFFER *gbuf)
{
    /*--------------------------*
     *  Using data in memory
     *--------------------------*/
    if(!gbuf->file_mode) {
        return gbuf->data_size - gbuf->tail;
    }

    /*--------------------------*
     *  Using data in file
     *--------------------------*/
    size_t freebytes = gbuf->max_disk_size - gbuf->tail;
    return freebytes;
}

/***************************************************************************
 *    Pon /quit debug
 ***************************************************************************/
PUBLIC int gbuf_trace_create_delete(BOOL enable)
{
    __trace_create_delete__ = enable;
    return 0;
}

/***************************************************************************
 *    Set label
 ***************************************************************************/
PUBLIC int gbuf_setlabel(GBUFFER *gbuf, const char *label)
{
    GBMEM_STR_DUP(gbuf->label, label)
    return 0;
}

/***************************************************************************
 *    Set label with lenght
 ***************************************************************************/
PUBLIC int gbuf_setnlabel(GBUFFER *gbuf, const char *label, size_t len)
{
    GBMEM_STR_NDUP(gbuf->label, label, len)
    return 0;
}

/***************************************************************************
 *    Get label
 ***************************************************************************/
PUBLIC const char *gbuf_getlabel(GBUFFER *gbuf)
{
    if(!gbuf) {
        return 0;
    }
    return gbuf->label;
}

/***************************************************************************
 *    Set mark
 ***************************************************************************/
PUBLIC int gbuf_setmark(GBUFFER *gbuf, int32_t mark)
{
    if(!gbuf) {
        return -1;
    }
    gbuf->mark = mark;
    return 0;
}

/***************************************************************************
 *    Get mark
 ***************************************************************************/
PUBLIC int32_t gbuf_getmark(GBUFFER *gbuf)
{
    return gbuf->mark;
}

/***************************************************************************
 *  Serialize GBUFFER
 ***************************************************************************/
PUBLIC json_t *gbuf_serialize(
    GBUFFER *gbuf  // not owned
)
{
    json_t *__gbuffer__ = json_object();
    const char *label = gbuf_getlabel(gbuf);
    json_t *jn_label = 0;
    if(label) {
        jn_label = json_string(label);
        if(!jn_label) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "json_string() FAILED",
                NULL
            );
            log_debug_dump(0, label, strlen(label), "Error json_string");
            jn_label = json_string("");
        }
    } else {
        jn_label = json_string("");
    }

    json_object_set_new(__gbuffer__, "label", jn_label);
    int32_t mark = gbuf_getmark(gbuf);
    json_object_set_new(__gbuffer__, "mark", json_integer(mark));

    /*
     *  WARNING TODO
     *  By the moment only accept pure string.
     *  I suppose that binary data will fail.
     *  2015-07-06: Yes, if it's not a true UTF-8 will fail.
     *  2016-10-09: convierto a base64, en vez de json
     */
    // TODO use chunks!!
    gbuf_incref(gbuf);
    GBUFFER *gbuf_base64 = gbuf_encodebase64(gbuf);
    char *data = gbuf_cur_rd_pointer(gbuf_base64);
    json_t *jn_bf = json_string(data);
    if(!jn_bf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot serialize gbuffer data, json_string() FAILED",
            NULL
        );
        jn_bf = json_string(""); // TODO must return null? or use the FUTURE binary option
    }
    json_object_set_new(__gbuffer__, "data", jn_bf);
    gbuf_decref(gbuf_base64);
    return __gbuffer__;
}

/***************************************************************************
 *  Deserialize GBUFFER
 ***************************************************************************/
PUBLIC void *gbuf_deserialize(json_t *jn)
{
    json_t *__gbuffer__ = jn;
    const char *label = kw_get_str(__gbuffer__, "label", "", KW_REQUIRED);
    int32_t mark = kw_get_int(__gbuffer__, "mark", 0, KW_REQUIRED);

    const char *base64 = kw_get_str(__gbuffer__, "data", "", KW_REQUIRED);
    GBUFFER *gbuf_decoded = gbuf_decodebase64string(base64);
    const char *data = gbuf_cur_rd_pointer(gbuf_decoded);

    int len = strlen(data);
    GBUFFER *gbuf;
    if(!len) {
        gbuf = gbuf_create(1, 1, 0, 0);
    } else {
        gbuf = gbuf_create(len, len, 0, 0);
    }
    if(!gbuf) {
        gbuf_decref(gbuf_decoded);
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "cannot deserialize gbuffer",
            NULL
        );
        return 0;
    }
    if(len) {
        gbuf_append(gbuf, (void *)data, len);
    }
    gbuf_setlabel(gbuf, label);
    gbuf_setmark(gbuf, mark);
    gbuf_decref(gbuf_decoded);
    return gbuf;
}

/***************************************************************************
 *  Load a file into a gbuffer
 *  TODO write a gbuf_load_file2 for big data with disk_size
 ***************************************************************************/
PUBLIC GBUFFER * gbuf_load_file(const char *path)
{
    if(empty_string(path)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Path empty",
            NULL
        );
        return 0;
    }
    if(access(path, 0)!=0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "File not found",
            "path",         "%s", path,
            NULL
        );
        return 0;
    }
    int fp= open(path, O_RDONLY);
    if(fp<0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open file",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return 0;
    }

    size_t size = lseek(fp, 0, SEEK_END);   // seek to end of file
    lseek(fp, 0, SEEK_SET);                 // seek back to beginning of file

    GBUFFER *gbuf = gbuf_create(size, size, 0, 0);
    if(!gbuf) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            "path",         "%s", path,
            "size",         "%d", size,
            NULL
        );
        close(fp);
        return 0;
    }

    int readed;
    char temp[BUFSIZ];
    while(1) {
        readed = read(fp, temp, sizeof(temp));
        if(readed == 0) {
            // EOF
            break;
        }
        if(readed < 0) {
            // Some error
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "read() FAILED",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            break;
        }
        gbuf_append(gbuf, temp, readed);
    }
    close(fp);
    return gbuf;
}

/***************************************************************************
 *  Save gbuffer to file
 *  gbuf own
 ***************************************************************************/
PUBLIC int gbuf2file(GBUFFER *gbuf, const char *path, int permission, BOOL overwrite)
{
    /*----------------------------*
     *  Create the file
     *----------------------------*/
    int fd = newfile(path, permission, overwrite);
    if(fd<0) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "newfile() FAILED",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        gbuf_decref(gbuf);
        return -1;
    }

    /*----------------------------*
     *  Write the data
     *----------------------------*/
    int ret = 0;
    size_t len;
    while((len=gbuf_chunk(gbuf))>0) {
        char *p = gbuf_get(gbuf, len);
        if(write(fd, p, len)!=len) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "write() FAILED",
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            ret = -1;
            break;
        }
    }
    close(fd);
    gbuf_decref(gbuf);
    return ret;
}

/***************************************************************************
 *  Convert a json message from gbuffer into a json struct.
 *  gbuf is stolen
 *  Return 0 if error
 ***************************************************************************/
PRIVATE size_t on_load_callback(void *buffer, size_t buflen, void *data)
{
    GBUFFER *gbuf = data;

    // TODO falta que elimine los comentarios como json_config()
    // pero gbuf_getline() no funciona si hay chunks
    size_t chunk = gbuf_chunk(gbuf);
    if(!chunk)
        return 0;
    if(chunk > buflen)
        chunk = buflen;
    memcpy(buffer, gbuf_get(gbuf, chunk), chunk);
    return chunk;
}

PUBLIC json_t * gbuf2json(
    GBUFFER *gbuf,  // WARNING gbuf own and data consumed
    int verbose     // 1 log, 2 log+dump
)
{
    size_t flags = JSON_DECODE_ANY|JSON_ALLOW_NUL; // DANGER ojo added in 3/Feb/2020
    json_error_t jn_error;
    json_t *jn_msg = json_load_callback(on_load_callback, gbuf, flags, &jn_error);

    if(!jn_msg) {
        if(verbose) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_JSON_ERROR,
                "msg",          "%s", "json_load_callback() FAILED",
                "error",        "%s", jn_error.text,
                NULL
            );
            if(verbose > 1) {
                gbuf_reset_rd(gbuf);
                log_debug_gbuf(
                    0,
                    gbuf,
                    "Bad json format"
                );
            }
        }
    }
    gbuf_decref(gbuf);
    return jn_msg;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int json_append2gbuf(
    GBUFFER *gbuf,
    json_t *jn  // owned
)
{
    char *str = json2str(jn);
    if(!str) {
        JSON_DECREF(jn);
        return -1;
    }
    int ret = gbuf_append(gbuf, str, strlen(str));
    gbuf_append(gbuf, "\n", 1);
    gbmem_free(str);
    JSON_DECREF(jn);
    return ret;
}

/***************************************************************************
 *      Dump json into gbuf
 ***************************************************************************/
PRIVATE int dump2gbuf(const char *buffer, size_t size, void *data)
{
    GBUFFER *gbuf = data;

    if(size > 0) {
        gbuf_append(gbuf, (void *)buffer, size);
    }
    return 0;
}
PUBLIC GBUFFER *json2gbuf(
    GBUFFER *gbuf,
    json_t *jn, // owned
    size_t flags)
{
    if(!gbuf) {
        gbuf = gbuf_create(32*1024, gbmem_get_maximum_block(), 0, codec_utf_8);
    }
    json_dump_callback(jn, dump2gbuf, gbuf, flags);
    JSON_DECREF(jn);
    return gbuf;
}

/*****************************************************************
 *      Log hexa dump gbuffer
 *  WARNING only print a chunk size of data.
 *****************************************************************/
PUBLIC void log_debug_gbuf(
    log_opt_t opt,
    GBUFFER *gbuf,
    const char *fmt,
    ...
)
{
    if(!gbuf) {
        return;
    }

    int len = gbuf_chunk(gbuf);
    char *bf = gbuf_cur_rd_pointer(gbuf);

    va_list ap;
    va_start(ap, fmt);
    log_debug_vdump(opt, bf, len, fmt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log hexa dump gbuffer
 *  WARNING only print a chunk size of data.
 *****************************************************************/
PUBLIC void log_debug_full_gbuf(
    log_opt_t opt,
    GBUFFER *gbuf,
    const char *fmt,
    ...
)
{
    if(!gbuf) {
        return;
    }

    int len = gbuf_totalbytes(gbuf);
    char *bf = gbuf_head_pointer(gbuf);

    va_list ap;
    va_start(ap, fmt);
    log_debug_vdump(opt, bf, len, fmt, ap);
    va_end(ap);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC GBUFFER *gbuf_string2base64(const char *src, size_t len)
{
    size_t output_len = b64_output_encode_len(len);

    GBUFFER *gbuf_output = gbuf_create(output_len, output_len, 0, 0);
    if(!gbuf_output) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            "len",          "%d", (int)len,
            NULL
        );
        return 0;
    }
    char *p = _gbuf_cur_wr_pointer(gbuf_output);
    size_t encoded = b64_encode(src, len, p, output_len);
    if(encoded == -1) {
        gbuf_decref(gbuf_output);
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            "len",          "%d", (int)len,
            NULL
        );
        return 0;

    }
    gbuf_set_wr(gbuf_output, encoded);
    return gbuf_output;
}


/*****************************************************************
 *
 *****************************************************************/
PUBLIC GBUFFER *gbuf_file2base64(const char *path)
{
    FILE *infile;
    infile = fopen(path, "rb");
    if(!infile) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot open file",
            "path",         "%s", path,
            NULL
        );
        return 0;
    }
    struct stat buf;
    fstat(fileno(infile), &buf);
    size_t len = buf.st_size;
    char *bf = gbmem_malloc(len);
    if(!bf) {
        fclose(infile);
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "No memory",
            "len",          "%d", (int)len,
            NULL
        );
        return 0;
    }
    fread(bf, len, 1, infile);
    fclose(infile);

    GBUFFER *gbuf_output = gbuf_string2base64(bf, len);
    gbmem_free(bf);
    return gbuf_output;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC GBUFFER *gbuf_encodebase64( // return new gbuffer
    GBUFFER *gbuf_input  // decref
)
{
    char *src = gbuf_cur_rd_pointer(gbuf_input);
    size_t len = gbuf_leftbytes(gbuf_input);
    GBUFFER *gbuf_output = gbuf_string2base64(src, len);
    gbuf_decref(gbuf_input);
    return gbuf_output;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC GBUFFER *gbuf_decodebase64stringn(const char* base64, int base64_len)
{
    size_t output_len = b64_output_decode_len(base64_len);

    GBUFFER *gbuf_output = gbuf_create(output_len, output_len, 0, 0);
    if(!gbuf_output) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            "len",          "%d", (int)output_len,
            NULL
        );
        return 0;
    }
    uint8_t *p = _gbuf_cur_wr_pointer(gbuf_output);
    size_t decoded = b64_decode(base64, p, output_len);
    if(decoded == -1) {
        gbuf_decref(gbuf_output);
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            "len",          "%d", (int)output_len,
            NULL
        );
        return 0;

    }
    gbuf_set_wr(gbuf_output, decoded);
    return gbuf_output;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC GBUFFER *gbuf_decodebase64string(const char *base64)
{
    return gbuf_decodebase64stringn(base64, strlen(base64));
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC GBUFFER *gbuf_decodebase64(
    GBUFFER *gbuf_input  // decref
)
{
    char *base64 = gbuf_cur_rd_pointer(gbuf_input);
    GBUFFER *gbuf_output = gbuf_decodebase64string(base64);
    gbuf_decref(gbuf_input);
    return gbuf_output;
}

/***************************************************************************
 *  Convert utf8 to unicode (wchar_t)
 ***************************************************************************/
PUBLIC GBUFFER *gbuf_utf8_to_unicode( // return new gbuffer
    GBUFFER *gbuf_input  // decref
)
{
    char *utf8 = gbuf_cur_rd_pointer(gbuf_input);

    size_t output_len = (strlen(utf8) + 1) * sizeof(wchar_t);

    GBUFFER *gbuf_output = gbuf_create(output_len, output_len, 0, 0);
    if(!gbuf_output) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            "len",          "%d", (int)output_len,
            NULL
        );
        gbuf_decref(gbuf_input);
        return 0;
    }
    wchar_t *p = _gbuf_cur_wr_pointer(gbuf_output);
    int ln = strlen(utf8);
    utf8proc_ssize_t decoded = utf8proc_decompose(
        (const utf8proc_uint8_t *)utf8,
        ln,
        (utf8proc_int32_t *)p,
        output_len,
        0
    );
    if(decoded == -1) {
        gbuf_decref(gbuf_output);
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "utf8proc_decompose() FAILED",
            "error",        "%s", utf8proc_errmsg(decoded),
            NULL
        );
        gbuf_decref(gbuf_input);
        return 0;
    }
    decoded *= sizeof(utf8proc_int32_t);
    if(decoded > output_len) {
        gbuf_decref(gbuf_output);
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "utf8proc_decompose() BAD size",
            "output_len",   "%d", (int)output_len,
            "decoded",      "%d", (int)decoded,
            NULL
        );
        gbuf_decref(gbuf_input);
        return 0;
    }
    gbuf_set_wr(gbuf_output, decoded);
    gbuf_decref(gbuf_input);
    return gbuf_output;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int stream_json_filename_parser(
    const char *path,
    json_stream_callback_t json_stream_callback,
    void *user_data,
    int verbose     // 1 log, 2 log+dump
)
{
    FILE *file = fopen(path, "r");
    if(!file) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Cannot open file",
                "path",         "%s", path,
                NULL
            );
        }
        return -1;
    }
    /*
     *  Load commands
     */
    #define WAIT_BEGIN_DICT 0
    #define WAIT_END_DICT   1
    int c;
    int st = WAIT_BEGIN_DICT;
    int brace_indent = 0;
    GBUFFER *gbuf = gbuf_create(16*1024, gbmem_get_maximum_block(), 0, 0);
    while((c=fgetc(file))!=EOF) {
        switch(st) {
        case WAIT_BEGIN_DICT:
            if(c != '{') {
                continue;
            }
            gbuf_clear(gbuf);
            gbuf_append(gbuf, &c, 1);
            brace_indent = 1;
            st = WAIT_END_DICT;
            break;
        case WAIT_END_DICT:
            if(c == '{') {
                brace_indent++;
            } else if(c == '}') {
                brace_indent--;
            }
            gbuf_append(gbuf, &c, 1);
            if(brace_indent == 0) {
                gbuf_incref(gbuf);
                json_t *jn_dict = gbuf2json(gbuf, verbose);
                gbuf_clear(gbuf);
                if(jn_dict) {
                    json_stream_callback(user_data, jn_dict);
                }
                st = WAIT_BEGIN_DICT;
            }
            break;
        }
    }
    fclose(file);
    gbuf_decref(gbuf);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int stream_json_filename_parser2(
    const char *path,
    json_stream_callback_t json_stream_callback,
    void *user_data,
    int verbose     // 1 log, 2 log+dump
)
{
    FILE *file = fopen(path, "r");
    if(!file) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Cannot open file",
                "path",         "%s", path,
                NULL
            );
        }
        return -1;
    }
    /*
     *  Load commands
     */
    #define WAIT_BEGIN_DICT 0
    #define WAIT_END_DICT   1
    int c;
    int begin_separator;
    int end_separator;
    int st = WAIT_BEGIN_DICT;
    int brace_indent = 0;
    GBUFFER *gbuf = gbuf_create(16*1024, gbmem_get_maximum_block(), 0, 0);
    while((c=fgetc(file))!=EOF) {
        switch(st) {
        case WAIT_BEGIN_DICT:
            if(!(c == '{' || c == '[')) {
                continue;
            }
            if(c == '{') {
                begin_separator = '{';
                end_separator = '}';
            } else {
                begin_separator = '[';
                end_separator = ']';
            }
            gbuf_clear(gbuf);
            gbuf_append(gbuf, &c, 1);
            brace_indent = 1;
            st = WAIT_END_DICT;
            break;
        case WAIT_END_DICT:
            if(c == begin_separator) {
                brace_indent++;
            } else if(c == end_separator) {
                brace_indent--;
            }
            gbuf_append(gbuf, &c, 1);
            if(brace_indent == 0) {
                gbuf_incref(gbuf);
                json_t *jn_dict = gbuf2json(gbuf, verbose);
                gbuf_clear(gbuf);
                if(jn_dict) {
                    json_stream_callback(user_data, jn_dict);
                }
                st = WAIT_BEGIN_DICT;
            }
            break;
        }
    }
    fclose(file);
    gbuf_decref(gbuf);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *stream_json_file_parser(
    FILE *file,     // Read until EOF of a new json dict.
    int verbose     // 1 log, 2 log+dump
)
{
    /*
     *  Load commands
     */
    #define WAIT_BEGIN_DICT 0
    #define WAIT_END_DICT   1
    int c;
    int st = WAIT_BEGIN_DICT;
    int brace_indent = 0;
    GBUFFER *gbuf = gbuf_create(4*1024, gbmem_get_maximum_block(), 0, 0);
    while((c=fgetc(file))!=EOF) {
        switch(st) {
        case WAIT_BEGIN_DICT:
            if(c != '{') {
                continue;
            }
            gbuf_clear(gbuf);
            gbuf_append(gbuf, &c, 1);
            brace_indent = 1;
            st = WAIT_END_DICT;
            break;
        case WAIT_END_DICT:
            if(c == '{') {
                brace_indent++;
            } else if(c == '}') {
                brace_indent--;
            }
            gbuf_append(gbuf, &c, 1);
            if(brace_indent == 0) {
                gbuf_incref(gbuf);
                json_t *jn_dict = gbuf2json(gbuf, verbose);
                if(jn_dict) {
                    gbuf_decref(gbuf);
                    return jn_dict;
                }
                st = WAIT_BEGIN_DICT;
            }
            break;
        }
    }
    gbuf_decref(gbuf);

    return 0;
}

/***************************************************************************
   Return the content of key inside a gbuffer. Numbers are converted to string.
 ***************************************************************************/
PUBLIC GBUFFER *kw_get_gbuf_value(
    json_t *jn_record,
    const char *key,
    GBUFFER *gbuf,  // If null create a new gbuf, else, return this.
    kw_flag_t flag
)
{
    char *s = 0;

    if(kw_has_path(jn_record, key)) {
        json_t *jn_value = kw_get_dict_value(jn_record, key, 0, 0);
        s = jn2string(jn_value);
    }

    if(s) {
        int ln = strlen(s) + 1;
        if(!gbuf) {
            gbuf = gbuf_create(ln, ln, 0, 0);
        }
        gbuf_append(gbuf, s, ln);
        gbmem_free(s);
    }
    return gbuf;
}

