/***********************************************************************
 *              CONVERT HEXADECIMAL library
 *              Copyright (c) 1995 Niyamaka.
 *              All Rights Reserved.
 ***********************************************************************/
#pragma once

#include "00_gtypes.h"
#include "01_print_error.h"

#ifdef __cplusplus
extern "C"{
#endif

/***********************************************************************
 *     Prototypes
 ***********************************************************************/
char *hex2bin(char *bf, int bfsize, const char *hex, size_t hex_len, size_t *out_len); // return bf
char *bin2hex(char *bf, int bfsize, const char *bin, size_t bin_len); // return bf

BOOL all_00(uint8_t *bf, int bfsize);
BOOL all_ff(uint8_t *bf, int bfsize);

typedef int (*view_fn_t)(const char *format, ...);
typedef int (*view_fn2_t)(void *user_data, int user_data2, const char *format, ...);

/*
 *  tdump  -> en hexa y en asci y con contador (indice)
 *  tdump1 -> solo en hexa
 *  tdump2 -> en hexa y en asci
 */
void tdump(const char *s, size_t len, view_fn_t view_fn);
void tdump1(const char *s, size_t len, view_fn_t view_fn);
void tdump2(const char *s, size_t len, view_fn_t view_fn);

void tdumps(const char *prefix, const char *s, size_t len, view_fn_t view_fn);
void tdumps1(const char *prefix, const char *s, size_t len, view_fn_t view_fn);
void tdumps2(const char *prefix, const char *s, size_t len, view_fn_t view_fn);

void tdumpsu(void *user_data, int user_data2, const char *prefix, const char *s, size_t len, view_fn2_t view_fn2);
void tdumpsu2(void *user_data, int user_data2, const char *prefix, const char *s, size_t len, view_fn2_t view_fn2);

#ifdef __cplusplus
}
#endif
