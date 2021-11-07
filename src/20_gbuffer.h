/****************************************************************************
 *              GBUFFER.H
 *              Buffer growable and overflowable
 *              Copyright (c) 2013 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <jansson.h>

/*
 *  Dependencies
 */
#include "00_base64.h"
#include "00_utf8.h"
#include "00_utf8proc.h"
#include "02_dirs.h"
#include "11_gbmem.h"
#include "14_kw.h"
#include "20_gbuffer.h"

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Constants
 *********************************************************************/
#define GBUF_DECREF(ptr)    \
    if(ptr) {               \
        gbuf_decref(ptr);   \
        (ptr) = 0;          \
    }

#define GBUF_INCREF(ptr)    \
    if(ptr) {               \
        gbuf_incref(ptr);   \
    }


#define GBUF_CREATE(var_gbuf, data_size, max_memory_size, max_disk_size, encoding)  \
    if(var_gbuf) {                                                                  \
        log_error(LOG_OPT_TRACE_STACK,                                              \
            "gobj",         "%s", __FILE__,                                         \
            "function",     "%s", __FUNCTION__,                                     \
            "process",      "%s", get_process_name(),                               \
            "hostname",     "%s", get_host_name(),                                  \
            "pid",          "%d", get_pid(),                                        \
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,                            \
            "msg",          "%s", "gbuf ALREADY exists!",                           \
            NULL                                                                    \
        );                                                                          \
        gbuf_decref(var_gbuf);                                                      \
    }                                                                               \
    (var_gbuf) = gbuf_create(data_size, max_memory_size, max_disk_size, encoding);

#define GBUF_DESTROY(ptr)   \
    if(ptr) {               \
        gbuf_decref(ptr);   \
    }                       \
    (ptr) = 0;


typedef enum _gbuf_encoding {
    codec_utf_8 = 0,
    codec_binary,
} gbuf_encoding;

/*********************************************************************
 *      Structures
 *********************************************************************/
typedef struct _GBUFFER {
    DL_ITEM_FIELDS

    size_t refcount;            /* to delete by reference counter */

    const char *label;          /* like user_data */
    int32_t mark;               /* like user_data */

    size_t data_size;           /* nº bytes allocated for data */
    size_t max_memory_size;     /* maximum size in memory */
    size_t max_disk_size;       /* maximum size in disk */
    gbuf_encoding encoding;

    /*
     *  Con tail controlo la entrada de bytes en el packet.
     *  El número total de bytes en el packet será tail.
     */
    size_t tail;    /* write pointer */
    size_t curp;    /* read pointer */

    /*
     *  Data dinamycally allocated
     *  In file_mode this buffer only is for read from file.
     */
    char *data;

    /*
     *  TODO optimize
     *  Instead of creating new gbuffers to insert data in head,
     *  use a linked list of gbuffers.
     */
    dl_list_t dl_gbuffers;

    /*
     *  Using file
     */
    char file_mode;     // True when using tmpfile
    FILE *tmpfile;
    char writting;      // True when writting: the pointer is at the tail position.
                        // False when reading: the pointer is at the curp position.

    /*
     *  Callback when a gbuffer is destroyed.
     */
    int (*on_destroy_gbuf)(void *param1, void *param2, void *param3, void *param4);
    void *param1;
    void *param2;
    void *param3;
    void *param4;
} GBUFFER;

/*********************************************************************
 *      PROTOTYPES
 *********************************************************************/
PUBLIC GBUFFER * gbuf_create(
    size_t data_size,
    size_t max_memory_size,
    size_t max_disk_size,
    gbuf_encoding encoding
);
PUBLIC void gbuf_remove(GBUFFER *gbuf); /* do not call gbuf_remove(), call gbuf_decref() */
PUBLIC void gbuf_incref(GBUFFER *gbuf);
PUBLIC void gbuf_decref(GBUFFER *gbuf);

/*
 *  HACK Último eslabón. Cuando destruyas un gbuffer, informa.
 *  Útil para el alto nivel, que quiera un ACK cuando su gbuffer es eliminado.
 *  Los callback como eventos please.
 */
PUBLIC void gbuf_set_on_destroy_gbuf(
    GBUFFER *gbuf,
    int (*on_destroy_gbuf)(void *param1, void *param2, void *param3, void *param4),
    void *param1,
    void *param2,
    void *param3,
    void *param4
);


/*
 *  READING
 */
PUBLIC void *gbuf_cur_rd_pointer(GBUFFER *gbuf);  /* Return current reading pointer */
PUBLIC void gbuf_reset_rd(GBUFFER *gbuf);        /* reset read pointer */

PUBLIC int gbuf_set_rd_offset(GBUFFER* gbuf, size_t position);
PUBLIC int gbuf_ungetc(GBUFFER* gbuf, char c);
PUBLIC size_t gbuf_get_rd_offset(GBUFFER* gbuf);
/*
 * Pop 'len' bytes, return the pointer.
 * If there is no `len` bytes of data to pop, return 0, and no data is popped.
 */
PUBLIC void *gbuf_get(GBUFFER *gbuf, size_t len);
PUBLIC char gbuf_getchar(GBUFFER *gbuf);   /* pop one bytes */
PUBLIC size_t gbuf_chunk(GBUFFER *gbuf);   /* return the chunk of data available */

PUBLIC void *gbuf_find(GBUFFER *gbuf, const char *pattern, int patter_len); // put read pointer in found pattern
PUBLIC char *gbuf_getline(GBUFFER *gbuf, char separator);

/*
 *  WRITTING
 */
PUBLIC void *gbuf_cur_wr_pointer(GBUFFER *gbuf);  /* Return current writing pointer */

PUBLIC void gbuf_reset_wr(GBUFFER *gbuf);     /* reset write pointer (empty write/read data) */
PUBLIC int gbuf_set_wr(GBUFFER* gbuf, size_t offset);
PUBLIC size_t gbuf_append(GBUFFER *gbuf, void *data, size_t len);   /* return bytes written */
PUBLIC size_t gbuf_append_string(GBUFFER *gbuf, const char *s);
PUBLIC size_t gbuf_append_char(GBUFFER *gbuf, char c);              /* return bytes written */
PUBLIC int gbuf_append_gbuf(GBUFFER *dst, GBUFFER *src);
PUBLIC int gbuf_printf(GBUFFER *gbuf, const char *format, ...);
PUBLIC int gbuf_vprintf(GBUFFER *gbuf, const char *format, va_list ap);

/*
 *  Util
 */

PUBLIC void *gbuf_head_pointer(GBUFFER *gbuf);  /* Return pointer to first position of data */
PUBLIC void gbuf_clear(GBUFFER *gbuf);  // Reset write/read pointers

PUBLIC size_t gbuf_leftbytes(GBUFFER *gbuf);   /* nº bytes remain of reading */
PUBLIC size_t gbuf_totalbytes(GBUFFER *gbuf);  /* total written bytes */
PUBLIC size_t gbuf_freebytes(GBUFFER *gbuf);   /* free space */

PUBLIC int gbuf_trace_create_delete(BOOL enable);

PUBLIC int gbuf_setlabel(GBUFFER *gbuf, const char *label);
PUBLIC int gbuf_setnlabel(GBUFFER *gbuf, const char *label, size_t len);
PUBLIC const char *gbuf_getlabel(GBUFFER *gbuf);

PUBLIC int gbuf_setmark(GBUFFER *gbuf, int32_t mark);
PUBLIC int32_t gbuf_getmark(GBUFFER *gbuf);


PUBLIC json_t* gbuf_serialize(
    GBUFFER *gbuf // not owned
);
PUBLIC void *gbuf_deserialize(json_t *jn);

PUBLIC GBUFFER * gbuf_load_file(const char *path);
PUBLIC int gbuf2file(
    GBUFFER *gbuf,  // WARNING own
    const char *path,
    int permission,
    BOOL overwrite
);
PUBLIC json_t *gbuf2json(
    GBUFFER *gbuf,  // WARNING gbuf own and data consumed
    int verbose     // 1 log, 2 log+dump
);
PUBLIC int json_append2gbuf(
    GBUFFER *gbuf,
    json_t *jn  // owned
);
PUBLIC GBUFFER *json2gbuf(
    GBUFFER *gbuf,
    json_t *jn, // owned
    size_t flags
);

PUBLIC void log_debug_gbuf(
    log_opt_t opt,
    GBUFFER *gbuf,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 3, 4)));
PUBLIC void log_debug_full_gbuf(
    log_opt_t opt,
    GBUFFER *gbuf,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 3, 4)));

/*
 *  Encode to base64
 */
PUBLIC GBUFFER *gbuf_string2base64(const char* src, size_t len); // base64 without newlines
PUBLIC GBUFFER *gbuf_file2base64(const char *path);
PUBLIC GBUFFER *gbuf_encodebase64( // return new gbuffer
    GBUFFER *gbuf  // decref
);


/*
 *  Decode from base64
 */
PUBLIC GBUFFER *gbuf_decodebase64stringn(const char* base64, int base64_len);
PUBLIC GBUFFER *gbuf_decodebase64string(const char* base64);
PUBLIC GBUFFER *gbuf_decodebase64( // return new gbuffer
    GBUFFER *gbuf // decref
);

/*
 *  Convert utf8 to unicode (wchar_t)
 */
PUBLIC GBUFFER *gbuf_utf8_to_unicode( // return new gbuffer
    GBUFFER *gbuf // decref
);


/*
 *  Json Stream Parsers
 */
typedef int (*json_stream_callback_t)(
    void *user_data,
    json_t *dict_found  // owned!!
);
PUBLIC int stream_json_filename_parser(
    const char *path,
    json_stream_callback_t json_stream_callback,
    void *user_data,
    int verbose     // 1 log, 2 log+dump
);
PUBLIC json_t *stream_json_file_parser(
    FILE *file,     // Read until EOF of a new json dict.
    int verbose     // 1 log, 2 log+dump
);

/**rst**
   Return the content of key inside a gbuffer. Numbers are converted to string.
**rst**/
PUBLIC GBUFFER *kw_get_gbuf_value(
    json_t *jn_record,
    const char *key,
    GBUFFER *gbuf,  // If null create a new gbuf, else, return this.
    kw_flag_t flag
);


#ifdef __cplusplus
}
#endif
