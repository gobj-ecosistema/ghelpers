/***********************************************************************
 *          GSTRINGS.C
 *
 *          String manipulation functions
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "01_gstrings.h"

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
 *  Extract parameter: delimited by blanks (\b\t) or quotes ('' "")
 *  The string is modified (nulls inserted)!
 ***************************************************************************/
PUBLIC char *get_parameter(char *s, char **save_ptr)
{
    char c;
    char *p;

    if(!s) {
        if(save_ptr) {
            *save_ptr = 0;
        }
        return 0;
    }
    /*
     *  Find first no-blank
     */
    while(1) {
        c = *s;
        if(c==0)
            return 0;
        if(!(c==' ' || c=='\t'))
            break;
        s++;
    }

    /*
     *  Check quotes
     */
    if(c=='\'' || c=='"') {
        p = strchr(s+1, c);
        if(p) {
            *p = 0;
            if(save_ptr) {
                *save_ptr = p+1;
            }
            return s+1;
        }
    }

    /*
     *  Find first blank
     */
    p = s;
    while(1) {
        c = *s;
        if(c==0) {
            if(save_ptr) {
                *save_ptr = 0;
            }
            return p;
        }
        if((c==' ' || c=='\t')) {
            *s = 0;
            if(save_ptr) {
                *save_ptr = s+1;
            }
            return p;
        }
        s++;
    }
}

/***************************************************************************
 *  Extract key=value or key='this value' parameter
 *  Return the value, the key in `key`
 *  The string is modified (nulls inserted)!
 ***************************************************************************/
PUBLIC char *get_key_value_parameter(char *s, char **key, char **save_ptr)
{
    char c;
    char *p;

    if(!s) {
        if(save_ptr) {
            *save_ptr = 0;
        }
        return 0;
    }
    /*
     *  Find first no-blank
     */
    while(1) {
        c = *s;
        if(c==0)
            return 0;
        if(!(c==' ' || c=='\t'))
            break;
        s++;
    }

    char *value = strchr(s, '=');
    if(!value) {
        if(key) {
            *key = 0;
        }
        if(save_ptr) {
            *save_ptr = s;
        }
        return 0;
    }
    *value = 0; // delete '='
    value++;
    left_justify(s);
    if(key) {
        *key = s;
    }
    s = value;
    c = *s;

    /*
     *  Check quotes
     */
    if(c=='\'' || c=='"') {
        p = strchr(s+1, c);
        if(p) {
            *p = 0;
            if(save_ptr) {
                *save_ptr = p+1;
            }
            return s+1;
        } else {
            if(save_ptr) {
                *save_ptr = 0;
            }
            return 0;
        }
    }

    /*
     *  Find first blank
     */
    p = s;
    while(1) {
        c = *s;
        if(c==0) {
            if(save_ptr) {
                *save_ptr = 0;
            }
            return p;
        }
        if((c==' ' || c=='\t')) {
            *s = 0;
            if(save_ptr) {
                *save_ptr = s+1;
            }
            return p;
        }
        s++;
    }
}

/***************************************************************************
 *  Convert string to upper case
 ***************************************************************************/
PUBLIC char *strtoupper(char* s)
{
    if(!s)
        return 0;

    char* p = s;
    while(*p != '\0') {
        *p = toupper(*p);
        p++;
    }

    return s;
}

/***************************************************************************
 *  Convert n bytes of string to upper case
 ***************************************************************************/
PUBLIC char *strntoupper(char* s, size_t n)
{
    if(!s || n == 0)
        return 0;

    char *p = s;
    while (n > 0 && *p != '\0') {
        *p = toupper(*p);
        p++;
        n--;
    }

    return s;
}

/***************************************************************************
 *  Convert string to lower case
 ***************************************************************************/
PUBLIC char *strtolower(char* s)
{
    if(!s)
        return 0;

    register char* p = s;
    while (*p != '\0') {
        *p = tolower(*p);
        p++;
    }

    return s;
}

/***************************************************************************
 *  Convert n bytes of string to lower case
 ***************************************************************************/
PUBLIC char *strntolower(char* s, size_t n)
{
    if(!s || n == 0)
        return 0;

    char *p = s;
    while (n > 0 && *p != '\0') {
        *p = tolower(*p);
        p++;
        n--;
    }

    return s;
}

/***************************************************************************
 *    Elimina blancos a la derecha. (Espacios, tabuladores, CR's  o LF's)
 ***************************************************************************/
PUBLIC void delete_right_blanks(char *s)
{
    int l;
    char c;

    /*---------------------------------*
     *  Elimina blancos a la derecha
     *---------------------------------*/
    l=strlen(s);
    if(l==0)
        return;
    while(--l>=0) {
        c= *(s+l);
        if(c==' ' || c=='\t' || c=='\n' || c=='\r')
            *(s+l)='\0';
        else
            break;
    }
}

/***************************************************************************
 *    Elimina el caracter 'x' a la derecha.
 ***************************************************************************/
PUBLIC char *delete_right_char(char *s, char x)
{
    int l;

    l = strlen(s);
    if(l==0) {
        return s;
    }
    while(--l>=0) {
        if(*(s+l)==x) {
            *(s+l)='\0';
        } else {
            break;
        }
    }
    return s;
}

/***************************************************************************
 *    Elimina el caracter 'x' a la izquierda.
 ***************************************************************************/
PUBLIC char *delete_left_char(char *s, char x)
{
    int l;
    char c;

    if(strlen(s)==0) {
        return s;
    }

    l=0;
    while(1) {
        c= *(s+l);
        if(c=='\0'){
            break;
        }
        if(c==x) {
            l++;
        } else {
            break;
        }
    }
    if(l>0) {
        memmove(s,s+l,strlen(s)-l+1);
    }
    return s;
}

/***************************************************************************
 *    Elimina blancos a la izquierda. (Espacios, tabuladores, CR's  o LF's)
 ***************************************************************************/
PUBLIC void delete_left_blanks(char *s)
{
    int l;
    char c;

    /*----------------------------*
     *  Busca el primer no blanco
     *----------------------------*/
    l=0;
    while(1) {
        c= *(s+l);
        if(c=='\0')
            break;
        if(c==' ' || c=='\t' || c=='\n' || c=='\r')
            l++;
        else
            break;
    }
    if(l>0)
        memmove(s,s+l,strlen(s)-l+1);
}

/***************************************************************************
 *    Justifica a la izquierda eliminado blancos a la derecha
 ***************************************************************************/
PUBLIC void left_justify(char *s)
{
    if(s) {
        /*---------------------------------*
         *  Elimina blancos a la derecha
         *---------------------------------*/
        delete_right_blanks(s);

        /*-----------------------------------*
         *  Quita los blancos a la izquierda
         *-----------------------------------*/
        delete_left_blanks(s);
    }
}

/****************************************************************************
 *  Translate characters from the string 'from' to the string 'to'.
 *  Usage:
 *        char * translate_string(char *to,
 *                                char *from,
 *                                char *mk_to,
 *                                char *mk_from);
 *  Description:
 *        Move characters from 'from' to 'to' using the masks
 *  Return:
 *        A pointer to string 'to'.
 ****************************************************************************/
PUBLIC char * translate_string(char *to, int tolen, const char *from, const char *mk_to, const char *mk_from)
{
    int pos;
    char chr;
    char *chr_pos;

    tolen--;
    if(tolen<1)
        return to;
    strncpy(to, mk_to, tolen);
    to[tolen] = 0;

    while((chr = *mk_from++) != 0) {
        if((chr_pos = strchr(mk_to,chr)) == NULL) {
            from++;
            continue ;
        }
        pos = (int) (chr_pos - mk_to);
        if(pos > tolen) {
            break;
        }
        to[pos++] = *from++ ;

        while(chr == *mk_from) {
            if(chr == mk_to[pos]) {
                if(pos > tolen) {
                    break;
                }
                to[pos++] = *from;
            }
            mk_from++ ;
            from++;
        }
    }
    return to;
}

/***************************************************************************
 *    cambia el caracter old_d por new_c. Retorna los caracteres cambiados
 ***************************************************************************/
PUBLIC int change_char(char *s, char old_c, char new_c)
{
    int count = 0;

    while(*s) {
        if(*s == old_c) {
            *s = new_c;
            count++;
        }
        s++;
    }
    return count;
}

/***************************************************************************
 *    Elimina el caracter c del string. Retorna los caracteres eliminados
 ***************************************************************************/
PUBLIC int remove_char(char *s, char c)
{
    int count = 0;
    char *p = s;
    while(*s) {
        if(*s == c) {
            s++;
            count++;
            continue;
        }
        *p = *s;
        p++;
        s++;
    }
    *p = 0;
    return count;
}

/***************************************************************************
 *    Cuenta cuantos caracteres de 'c' hay en 's'
 ***************************************************************************/
PUBLIC int count_char(const char *s, char c)
{
    int count = 0;

    while(*s) {
        if(*s == c)
            count++;
        s++;
    }
    return count;
}

/***************************************************************************
 *  lstrip - return pointer to string position skipping all leading `c` chars
 ***************************************************************************/
PUBLIC const char *lstrip(const char* s, size_t str_size, char c)
{
    if (str_size == 0)
        return s;

    const char *p = s;
    while (str_size > 0 && *p != '\0') {
        if(*p != c)
            return p;
        p++;
        str_size--;
    }

    return s;
}

/***************************************************************************
 *  lnstrip - return pointer to string position skipping `n` `c` chars
 ***************************************************************************/
PUBLIC const char *lnstrip(const char* s, size_t str_size, char c, int *n)
{
    *n = 0;
    if (str_size == 0)
        return s;

    const char *p = s;
    while (str_size > 0 && *p != '\0') {
        if(*p != c) {
            (*n)++;
            return p;
        }
        p++;
        str_size--;
    }
    return s;
}

/***************************************************************************
 *  Return TRUE if str is in string list.
 ***************************************************************************/
PUBLIC BOOL str_in_list(const char **list, const char *str, BOOL ignore_case)
{
    int (*cmp_fn)(const char *s1, const char *s2)=0;
    if(ignore_case) {
        cmp_fn = strcasecmp;
    } else {
        cmp_fn = strcmp;
    }

    while(*list) {
        if(cmp_fn(str, *list)==0) {
            return TRUE;
        }
        list++;
    }
    return FALSE;
}

/***************************************************************************
 *  Return idx of str in string list. Based in 1. Return 0 if not found.
 ***************************************************************************/
PUBLIC int idx_in_list(const char **list, const char *str, BOOL ignore_case)
{
    int (*cmp_fn)(const char *s1, const char *s2)=0;
    if(ignore_case) {
        cmp_fn = strcasecmp;
    } else {
        cmp_fn = strcmp;
    }

    int i = 0;
    while(*list) {
        if(cmp_fn(str, *list)==0) {
            return i+1;
        }
        i++;
        list++;
    }
    return 0;
}

/***************************************************************************
 *  Return TRUE if int is in string list.
 ***************************************************************************/
PUBLIC BOOL int_in_str_list(const char **list, int i)
{
    while(*list) {
        if(atoi(*list)==i) {
            return TRUE;
        }
        list++;
    }
    return FALSE;
}

/****************************************************************************
 *  Find String
 *  src          -> string en el que buscar 's'.
 *  src_len      -> longitud de 'src'
 *  s            -> string a buscar
 *  s_len        -> longitud de 's'
 *  Retorna un pointer al string encontrado, o NULL si no lo ha encontrado
 ****************************************************************************/
PUBLIC char * mem_find(const char *buffer, int buffer_len, const char *s, int s_len)
{
    register const char *src;

    src = buffer;
    while(buffer_len > 0 && buffer_len >= s_len) {
        if(memcmp(src, s, s_len) == 0) {
            return (char *)src;
        }
        src++;
        buffer_len--;
    }
    return (char *)0;
}

/****************************************************************************
 *  Encuentra un char, n veces
 *  src          -> string en el que buscar 'c'.
 *  src_len      -> longitud de 'src'
 *  c            -> char a buscar
 *  n            -> veces que se busca c
 *  Retorna un pointer al string encontrado, o NULL si no lo ha encontrado
 ****************************************************************************/
PUBLIC char * mem_find_nchar(char *buffer, int buffer_len, char c, int n)
{
    register char *src;
    int veces = 0;

    src = buffer;
    while(buffer_len > 0) {
        if(*src == c) {
            veces++;
            if(veces == n)
                return src;
        }
        src++;
        buffer_len--;
    }
    return (char *)0;
}

/****************************************************************************
    Count number of occurrences of a given subdata in a data
    buffer          -> data en el que buscar 's'.
    buffer_len      -> longitud de 'data'
    s            -> subdata a buscar
    s_len        -> longitud de 's'
    Return number of occurrences
 ****************************************************************************/
PUBLIC int mem_counter(const char *buffer, int buffer_len, const char *s, int s_len)
{
    int counter = 0;

    register const char *src;

    src = buffer;
    while(buffer_len > 0 && buffer_len >= s_len) {
        if(memcmp(src, s, s_len) == 0) {
            counter++;
        }
        src++;
        buffer_len--;
    }
    return counter;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL all_numbers(const char* s)
{
    if(empty_string(s))
        return 0;

    const char* p = s;
    while(*p != '\0') {
        if(!isdigit(*p)) {
            return FALSE;
        }
        p++;
    }
    return TRUE;
}

