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

#if defined(_WIN32) || defined(_WINDOWS)
    #include <process.h>
    #define snprintf _snprintf
    #define vsnprintf _vsnprintf
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define O_LARGEFILE 0
    #define O_NOFOLLOW 0
    #define open _open
    #define close _close
    #define lseek64 _lseek
    #define fseeko64 _fseeki64
    #define lseek _lseek
    #define read _read
    #define write _write
    #define access _access

    #define fopen64 fopen
    #define tmpfile64 tmpfile
    #define ftello64 _ftelli64
    #define tmpfile64 tmpfile
    #define fileno _fileno
    #define getpid _getpid
    #define unlink _unlink
    #define popen _popen
    #define pclose _pclose
    #define isatty _isatty
    #define chmod _chmod
    #define umask _umask
    #define strdup _strdup
    #define getpid _getpid
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
