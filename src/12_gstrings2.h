/****************************************************************************
 *          GSTRINGS2.H
 *
 *          String manipulation functions with gbmem
 *
 *          Copyright (c) 2017 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/


#ifndef _C_GSTRINGS2_H
#define _C_GSTRINGS2_H 1

#include <string.h>
#include <regex.h>
#include <stdbool.h>

#include "11_gbmem.h"

#ifdef __cplusplus
extern "C"{
#endif


/***************************************************
 *              Prototypes
 **************************************************/
/**rst**

.. function:: int split(const char *str, const char *delim, char **list, int list_size)

   Split a string by delimiter and save in `list`,
   a user pointer array of `list_size` size.
   The maximum items of list items is `list_size`.
   The maximum length of a string item is BUFSIZ.
   WARNING: Remember free **list content with split_free().
   Return the list length.

**rst**/
PUBLIC int split(const char *str, const char *delim, char **list, int list_size);

/**rst**

.. function:: void split_free(char **list, int list_size)

   Free split list content

**rst**/
PUBLIC void split_free(char **list, int list_size);

/**rst**
    Split a string by delim returning the list of strings.
    Fill list_size if not null.
    WARNING Remember free with split_free2().
**rst**/
PUBLIC const char ** split2(const char *str, const char *delim, int *list_size);
PUBLIC void split_free2(const char **list);

/**rst**
    Get a substring from begin to end, like python
    WARNING Remember free with substring_free().
**rst**/
PUBLIC char * substring(const char *str, int begin, int end);
PUBLIC void substring_free(char *s);

/**rst**
    Concat two strings or three strings
    WARNING Remember free with str_concat_free().
**rst**/
PUBLIC char * str_concat(const char *str1, const char *str2);
PUBLIC char * str_concat3(const char *str1, const char *str2, const char *str3);
PUBLIC void str_concat_free(char *s);


#ifdef __cplusplus
}
#endif

#endif

