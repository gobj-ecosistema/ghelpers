/****************************************************************************
 *              PRINT_ERROR.H
 *              Copyright (c) 2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/

#ifndef _PRINT_ERROR_H
#define _PRINT_ERROR_H 1

#include <stdarg.h>
#include "00_gtypes.h"

#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Constants
 *****************************************************************/
typedef enum {
    PEF_CONTINUE    = 0,
    PEF_EXIT        = -1,
    PEF_ABORT       = -2,
    PEF_SILENCE     = -3
} pe_flag_t;

/*****************************************************************
 *     Prototypes
 *****************************************************************/

/*
 *  print an error message in syslog and console.
 */
PUBLIC void print_error(pe_flag_t quit, const char *prefix, const char *fmt, ...);
PUBLIC void print_info(const char *prefix, const char *fmt, ...);

/*
 *  Return a string description of the last error
 */
PUBLIC const char *get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* _PRINT_ERROR_H */

