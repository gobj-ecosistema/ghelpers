/***********************************************************************
 *              00_GTYPES.H
 *              Copyright (c) 2000-2015 Niyamaka.
 *              All Rights Reserved.
 *              Own type definitions
 ***********************************************************************/

#ifndef _GTYPES_H
#define _GTYPES_H 1

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <linux/limits.h>

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

#define UTF8_TO_UNICODE(dst, src) \
    utf8proc_decompose((const utf8proc_uint8_t *)src, strlen(src), dst, sizeof(dst), 0)

#define UNICODE_TO_UTF8(wide) \
    utf8proc_reencode((utf8proc_int32_t *)wide, wcslen((const wchar_t *)wide), 0)
#define UNICODE_TO_UTF8_2(dst,src) \
    u8_toutf8(dst, sizeof(dst), (const uint32_t *)src,  wcslen(src));

#define TEXT_NOT_NULL(s) (s)?(s):""


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

#endif /* _GTYPES_H */
