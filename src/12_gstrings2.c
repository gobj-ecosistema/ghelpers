/***********************************************************************
 *          GSTRINGS2.C
 *
 *          String manipulation functions with gbmem
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "12_gstrings2.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************************
   Split a string by delimiter and save in `list`,
   a user pointer array of `list_size` size.
   The maximum items of list items is `list_size`.
   The maximum length of a string item is BUFSIZ.
   WARNING: Remember free **list content with split_free().
   Return the list length.
 ***************************************************************************/
PUBLIC int split(const char *str, const char *delim, char **list, int list_size)
{
    int list_length;
    char *ptr;
    char *buffer = gbmem_strdup(str);
    if(!buffer) {
        return 0;
    }

    memset(list, 0, sizeof(char *) * list_size);
    list_length = 0;
    for (ptr = strtok(buffer, delim); ptr != NULL; ptr = strtok(NULL, delim)) {
        if (list_length < list_size) {
            list[list_length++] = gbmem_strdup(ptr);
        }
    }
    gbmem_free(buffer);
    return list_length;
}

/***************************************************************************
 *  Free split list content
 ***************************************************************************/
PUBLIC void split_free(char **list, int list_size)
{
    if(list) {
        for(int i=0; i<list_size; i++) {
            if(*list) {
                gbmem_free(*list);
                *list = 0;
                list++;
            }
        }
    }
}

/***************************************************************************
    Split a string by delim returning the list of strings.
    Fill list_size if not null.
    WARNING Remember free with split_free2().
 ***************************************************************************/
PUBLIC const char ** split2(const char *str, const char *delim, int *plist_size)
{
    char *ptr;

    if(plist_size) {
        *plist_size = 0; // error case
    }
    char *buffer = gbmem_strdup(str);
    if(!buffer) {
        return 0;
    }

    // Get list size
    int list_size = 0;
    for (ptr = strtok(buffer, delim); ptr != NULL; ptr = strtok(NULL, delim)) {
        list_size++;
    }
    gbmem_free(buffer);

    buffer = gbmem_strdup(str);   // Prev buffer is destroyed!
    if(!buffer) {
        return 0;
    }
    // Alloc list
    int size = sizeof(char *) * (list_size + 1);
    const char **list = gbmem_malloc(size);

    // Fill list
    int i = 0;
    for (ptr = strtok(buffer, delim); ptr != NULL; ptr = strtok(NULL, delim)) {
        if (i < list_size) {
            list[i++] = gbmem_strdup(ptr);
        }
    }
    gbmem_free(buffer);

    if(plist_size) {
        *plist_size = i;
    }
    return list;
}

/***************************************************************************
 *  Free split list content
 ***************************************************************************/
PUBLIC void split_free2(const char **list)
{
    if(list) {
        char **p = (char **)list;
        while(*p) {
            gbmem_free(*p);
            *p = 0;
            p++;
        }
        gbmem_free(list);
    }
}

/***************************************************************************
    Split a string by delim returning the list of strings.
    Fill list_size if not null.
    WARNING Remember free with split_free3().
    HACK split() and split2() don't include the empty strings! this yes.
 ***************************************************************************/
PUBLIC const char ** split3(const char *str, const char *delim, int *plist_size)
{
    char *ptr, *p;

    if(plist_size) {
        *plist_size = 0; // error case
    }
    char *buffer = gbmem_strdup(str);
    if(!buffer) {
        return 0;
    }

    // Get list size
    int list_size = 0;

    #ifndef __linux__
    int TODO; // strsep in windows
    #endif

    p = buffer;
    while ((ptr = strsep(&p, delim)) != NULL) {
        list_size++;
    }
    gbmem_free(buffer);

    buffer = gbmem_strdup(str);   // Prev buffer is destroyed!
    if(!buffer) {
        return 0;
    }
    // Alloc list
    int size = sizeof(char *) * (list_size + 1);
    const char **list = gbmem_malloc(size);

    // Fill list
    int i = 0;
    p = buffer;
    while ((ptr = strsep(&p, delim)) != NULL) {
        if (i < list_size) {
            list[i] = gbmem_strdup(ptr);
            i++;
        }
    }
    gbmem_free(buffer);

    if(plist_size) {
        *plist_size = i;
    }
    return list;
}

/***************************************************************************
 *  Free split list content
 ***************************************************************************/
PUBLIC void split_free3(const char **list)
{
    if(list) {
        char **p = (char **)list;
        while(*p) {
            gbmem_free(*p);
            *p = 0;
            p++;
        }
        gbmem_free(list);
    }
}

/***************************************************************************
    Get a substring from begin to end, like python
    WARNING Remember gbmem_free with gbmem_free().
 ***************************************************************************/
PUBLIC char * substring(const char *str, int begin, int end)
{
    if(!str) {
        return 0;
    }
    int size = strlen(str);
    const char *p = 0;
    const char *f = 0;
    if(begin >= 0) {
        if(begin < size) {
            p = str + begin;
        } else {
            p = str + size - 1;
        }
    } else {
        if(-begin < size) {
            p = str + size + begin;
        } else {
            p = str;
        }
    }

    if(end >= 0) {
        if(end < size) {
            f = str + end;
        } else {
            f = str + size - 1;
        }
    } else {
        if(-end < size) {
            f = str + size + end;
        } else {
            f = str;
        }
    }

    char *subs = 0;
    if(p && f && f >= p) {
        int l = f - p + 1;
        subs = gbmem_malloc(l+1);
        memmove(subs, p, l);
    }
    return subs;
}

/***************************************************************************
 *  Free substring
 ***************************************************************************/
PUBLIC void substring_free(char *s)
{
    gbmem_free(s);
}

/***************************************************************************
    Concat two strings
    WARNING Remember free with str_concat_free().
 ***************************************************************************/
PUBLIC char * str_concat(const char *str1, const char *str2)
{
    int len = 0;

    if(str1) {
        len += strlen(str1);
    }
    if(str2) {
        len += strlen(str2);
    }

    char *s = gbmem_malloc(len+1);
    if(!s) {
        return NULL;
    }
    if(str1) {
        strcat(s, str1);
    }
    if(str2) {
        strcat(s, str2);
    }

    return s;
}

/***************************************************************************
    Concat three strings
    WARNING Remember free with str_concat_free().
 ***************************************************************************/
PUBLIC char * str_concat3(const char *str1, const char *str2, const char *str3)
{
    int len = 0;

    if(str1) {
        len += strlen(str1);
    }
    if(str2) {
        len += strlen(str2);
    }
    if(str3) {
        len += strlen(str3);
    }

    char *s = gbmem_malloc(len+1);
    if(!s) {
        return NULL;
    }
    if(str1) {
        strcat(s, str1);
    }
    if(str2) {
        strcat(s, str2);
    }
    if(str3) {
        strcat(s, str3);
    }

    return s;
}

/***************************************************************************
 *  Free concat
 ***************************************************************************/
PUBLIC void str_concat_free(char *s)
{
    gbmem_free(s);
}
