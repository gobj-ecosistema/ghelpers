/***********************************************************************
 *              00_GTYPES.H
 *              Copyright (c) 2000-2015 Niyamaka.
 *              All Rights Reserved.
 *              Own type definitions
 ***********************************************************************/
#pragma once

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"{
#endif

/*----------------------------------------------------------*
 *      General and common definitions
 *----------------------------------------------------------*/
typedef int         BOOL;
typedef void *      PTR;

#if defined(_WIN32) || defined(_WIN64)
  #define snprintf _snprintf
  #define vsnprintf _vsnprintf
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
#endif

#ifndef MIN
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
# define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef ARRAY_NSIZE
# define ARRAY_NSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define FALSE 0
#define TRUE  1

#define PRIVATE static

#  if defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 303
#    define PUBLIC __attribute__ ((visibility("default")))
#  else
#    define PUBLIC
#  endif

#ifndef NAME_MAX
#define NAME_MAX         255    /* # chars in a file name */
#endif

#ifndef PATH_MAX
#define PATH_MAX        4096    /* # chars in a path name including nul */
#endif

#ifndef BUFSIZ
#define BUFSIZ          4096    /* chosen to make stream I/O efficient */
#endif

#define EXEC_AND_RESET(funcion, variable) if(variable) {funcion(variable); variable=0;}

/*********************************************************************
 *      Prototypes of ghelpers.c
 *********************************************************************/
PUBLIC int set_process_name2(const char *role, const char *name);
PUBLIC const char *get_process_name(void);
PUBLIC const char *get_host_name(void);
PUBLIC const char *get_user_name(void);
PUBLIC int get_pid(void);


#ifdef __cplusplus
}
#endif
