/* buffer.c -- Auto-growing string buffers
 *
 * Copyright (c) 2013 Ginés Martínez
 *
 * Copyright (c) 2012 BalaBit IT Security Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BALABIT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BALABIT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "02_json_buffer.h"

/*****************************************************************
 *          Constants
 *****************************************************************/

/*****************************************************************
 *          Structures
 *****************************************************************/
typedef struct {
    size_t alloc;
    size_t len;
    char *msg;
    char indented;
    int items;
} ul_buffer_t;

/*****************************************************************
 *          Data
 *****************************************************************/
static const unsigned char json_exceptions[] = {
    0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
    0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
    0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e,
    0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
    0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
    0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,
    0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
    0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce,
    0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
    0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
    0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
    0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
    0xff, '\0'
};

PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */

/*
 *  Máximum # of json buffer.
 */
PRIVATE int max_hgen = 0;
#define MAX_UL_BUFFER 20
PRIVATE ul_buffer_t escape_buffer[MAX_UL_BUFFER];
PRIVATE ul_buffer_t ul_buffer[MAX_UL_BUFFER];

/*****************************************************************
 *          Prototypes
 *****************************************************************/
PRIVATE ul_buffer_t *ul_buffer_reset(hgen_t hgen, int indented);
PRIVATE ul_buffer_t *ul_buffer_append(
    hgen_t hgen,
    const char *key,
    const char *value,
    int with_comillas
);
PRIVATE char *ul_buffer_finalize(hgen_t hgen);

PRIVATE void ul_buffer_finish(void);

/*****************************************************************
 *
 *****************************************************************/
PRIVATE void ul_buffer_finish(void)
{
    for(int i=0; i<MAX_UL_BUFFER; i++) {
        if(escape_buffer[i].msg) {
            free(escape_buffer[i].msg);
            escape_buffer[i].msg = 0;
        }
    }
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE char *_ul_str_escape(
    const char *str,
    char *dest,
    size_t * length)
{
    const unsigned char *p;
    char *q;
    static unsigned char exmap[256];
    static int exmap_inited;

    if (!str)
        return NULL;

    p = (unsigned char *) str;
    q = dest;

    if (!exmap_inited) {
        const unsigned char *e = json_exceptions;

        memset(exmap, 0, 256);
        while (*e) {
            exmap[*e] = 1;
            e++;
        }
        exmap_inited = 1;
    }

    while (*p) {
        if (exmap[*p])
            *q++ = *p;
        else {
            switch (*p) {
            case '\b':
                *q++ = '\\';
                *q++ = 'b';
                break;
            case '\f':
                *q++ = '\\';
                *q++ = 'f';
                break;
            case '\n':
                *q++ = '\\';
                *q++ = 'n';
                break;
            case '\r':
                *q++ = '\\';
                *q++ = 'r';
                break;
            case '\t':
                *q++ = '\\';
                *q++ = 't';
                break;
            case '\\':
                *q++ = '\\';
                *q++ = '\\';
                break;
            case '"':
                *q++ = '\\';
                *q++ = '"';
                break;
            default:
                if ((*p < ' ') || (*p >= 0177)) {
                    const char *json_hex_chars = "0123456789abcdef";

                    *q++ = '\\';
                    *q++ = 'u';
                    *q++ = '0';
                    *q++ = '0';
                    *q++ = json_hex_chars[(*p) >> 4];
                    *q++ = json_hex_chars[(*p) & 0xf];
                } else
                    *q++ = *p;
                break;
            }
        }
        p++;
    }

    *q = 0;
    if (length)
        *length = q - dest;
    return dest;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE inline ul_buffer_t *_ul_buffer_ensure_size(
    ul_buffer_t * buffer, size_t size)
{
    if (buffer->alloc < size) {
        buffer->alloc += size * 2;
        char *msg = realloc(buffer->msg, buffer->alloc);
        if (!msg) {
            print_error(
                PEF_ABORT,
                "ERROR YUNETA",
                "realloc() FAILED %d bytes",
                buffer->alloc
            );
            return NULL;
        }
        buffer->msg = msg;
    }
    return buffer;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE ul_buffer_t *ul_buffer_reset(hgen_t hgen, int indented)
{
    ul_buffer_t * buffer = &ul_buffer[hgen];
    _ul_buffer_ensure_size(buffer, 512);
    _ul_buffer_ensure_size(&escape_buffer[hgen], 256);
    if(indented) {
        memcpy(buffer->msg, "{\n", 2);
        buffer->len = 2;
    } else {
        memcpy(buffer->msg, "{", 1);
        buffer->len = 1;
    }
    buffer->indented = indented;
    buffer->items = 0;
    return buffer;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE ul_buffer_t *ul_buffer_append(hgen_t hgen,
    const char *key,
    const char *value,
    int with_comillas)
{
    char *k, *v;
    size_t lk, lv;
    ul_buffer_t * buffer = &ul_buffer[hgen];
    size_t orig_len = buffer->len;

    /* Append the key to the buffer */
    escape_buffer[hgen].len = 0;
    _ul_buffer_ensure_size(&escape_buffer[hgen], strlen(key) * 6 + 1);
    k = _ul_str_escape(key, escape_buffer[hgen].msg, &lk);
    if (!k) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "_ul_str_escape() FAILED %s-%d",
            __FUNCTION__, __LINE__
        );
        return NULL;
    }

    buffer = _ul_buffer_ensure_size(buffer, buffer->len + lk + 10);
    if (!buffer)
        return NULL;

    if(buffer->items > 0) {
        memcpy(buffer->msg + buffer->len, ",", 1);
        buffer->len += 1;
        if(buffer->indented) {
            memcpy(buffer->msg + buffer->len, "\n", 1);
            buffer->len += 1;
        } else {
            memcpy(buffer->msg + buffer->len, " ", 1);
            buffer->len += 1;
        }
    }
    if(buffer->indented) {
        memcpy(buffer->msg + buffer->len, "    \"", 5);
        buffer->len += 5;
    } else {
        memcpy(buffer->msg + buffer->len, "\"", 1);
        buffer->len += 1;
    }

    memcpy(buffer->msg + buffer->len, k, lk);
    buffer->len += lk;
    if(with_comillas) {
        memcpy(buffer->msg + buffer->len, "\": \"", 4);
        buffer->len += 4;
    } else {
        memcpy(buffer->msg + buffer->len, "\": ", 3);
        buffer->len += 3;
    }

    /* Append the value to the buffer */
    escape_buffer[hgen].len = 0;
    _ul_buffer_ensure_size(&escape_buffer[hgen], strlen(value) * 6 + 1);
    v = _ul_str_escape(value, escape_buffer[hgen].msg, &lv);
    if (!v) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "_ul_str_escape() FAILED %s-%d",
            __FUNCTION__, __LINE__
        );
        buffer->len = orig_len;
        return NULL;
    }

    buffer = _ul_buffer_ensure_size(buffer, buffer->len + lv + 6);
    if (!buffer) {
        // PVS-Studio
        // buffer->len = orig_len;
        return NULL;
    }

    memcpy(buffer->msg + buffer->len, v, lv);
    buffer->len += lv;
    if(with_comillas) {
        memcpy(buffer->msg + buffer->len, "\"", 1);
        buffer->len += 1;
    }
    buffer->items++;
    return buffer;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE char *ul_buffer_finalize(hgen_t hgen)
{
    ul_buffer_t *buffer = &ul_buffer[hgen];
    if (!_ul_buffer_ensure_size(buffer, buffer->len + 3)) {
        return NULL;
    }
    if(buffer->indented)
        buffer->msg[buffer->len++] = '\n';
    else
        buffer->msg[buffer->len++] = ' ';
    buffer->msg[buffer->len++] = '}';
    buffer->msg[buffer->len] = '\0';
    return buffer->msg;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC hgen_t json_dict(void)
{
    hgen_t hgen;

    if (!atexit_registered) {
        atexit(ul_buffer_finish);
        atexit_registered = 1;
    }

    max_hgen++;
    hgen = max_hgen;
    if(max_hgen >= MAX_UL_BUFFER) {
        print_error(
            PEF_ABORT,
            "ERROR YUNETA",
            "json_dict(): NO MORE BUFFER"
        );
        return 0;
    }
    ul_buffer_reset(hgen, FALSE);
    return hgen;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC void json_reset(hgen_t hgen, BOOL indented)
{
    ul_buffer_reset(hgen, indented);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC char * json_get_buf(hgen_t hgen)
{
    return ul_buffer_finalize (hgen);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC void json_add_string(hgen_t hgen, char *key, char *str)
{
    ul_buffer_append(hgen, key, str, 1);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC void json_add_null(hgen_t hgen, char *key)
{
    ul_buffer_append(hgen, key, "null", 0);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC void json_add_double(hgen_t hgen, char *key, double number)
{
    char temp[64];

    snprintf(temp, sizeof(temp), "%.20g", number);
    if (strspn(temp, "0123456789-") == strlen(temp)) {
        strcat(temp, ".0");
    }
    ul_buffer_append(hgen, key, temp, 0);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC void json_add_integer(hgen_t hgen, char *key, long long int number)
{
    char temp[64];

    snprintf(temp, sizeof(temp), "%lld", number);
    ul_buffer_append(hgen, key, temp, 0);
}

