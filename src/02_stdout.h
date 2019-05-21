/****************************************************************************
 *          STDOUT.H
 *
 *          GLogger handler for stdout
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/

#ifndef _C_STDOUT_H
#define _C_STDOUT_H 1

#include <stdarg.h>
#include "01_print_error.h"

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  Syslog priority definitions
 */
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */
/*
 *  Extra mine priority definitions
 */
#define LOG_AUDIT       8       // written without header
#define LOG_MONITOR     9

/***************************************************
 *              Prototypes
 **************************************************/
PUBLIC int stdout_write(void *v, int priority, const char *bf, size_t len);
PUBLIC int stdout_fwrite(void* v, int priority, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
