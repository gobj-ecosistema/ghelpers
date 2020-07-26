/****************************************************************************
 *          GSTRINGS2.H
 *
 *          String manipulation functions
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/


#ifndef _C_GSTRINGS_H
#define _C_GSTRINGS_H 1

#include <string.h>
#include <regex.h>
#include <stdbool.h>

#include "00_gtypes.h"

#ifdef __cplusplus
extern "C"{
#endif

/***********************************************************************
 *  Reverse an array
 ***********************************************************************/
#define REVERSE_ARRAY(type, array, length)      \
    if (length > 1) {                           \
        for (int i = 0; i < length / 2; ++i) {  \
            type xxx;                           \
            xxx = array[i];                     \
            array[i] = array[length - i - 1];   \
            array[length - i - 1] = xxx;        \
        }                                       \
    }


/***********************************************************************
 *  Macros of switch for strings, copied from:
 *  https://gist.github.com/HoX/abfe15c40f2d9daebc35

Example:

int main(int argc, char **argv) {
     SWITCHS(argv[1]) {
        CASES("foo")
        CASES("bar")
            printf("foo or bar (case sensitive)\n");
            break;

        ICASES("pi")
            printf("pi or Pi or pI or PI (case insensitive)\n");
            break;

        CASES_RE("^D.*",0)
            printf("Something that start with D (case sensitive)\n");
            break;

        CASES_RE("^E.*",REG_ICASE)
            printf("Something that start with E (case insensitive)\n");
            break;

        CASES("1")
            printf("1\n");

        CASES("2")
            printf("2\n");
            break;

        DEFAULTS
            printf("No match\n");
            break;
    } SWITCHS_END;

    return 0;
}

 ***********************************************************************/
/** Begin a switch for the string x */
#define SWITCHS(x) \
    { const char *__sw = (x); bool __done = false; bool __cont = false; \
        regex_t __regex; regcomp(&__regex, ".*", 0); do {

/** Check if the string matches the cases argument (case sensitive) */
#define CASES(x)    } if ( __cont || !strcmp ( __sw, x ) ) \
                        { __done = true; __cont = true;

/** Check if the string matches the icases argument (case insensitive) */
#define ICASES(x)    } if ( __cont || !strcasecmp ( __sw, x ) ) { \
                        __done = true; __cont = true;

/** Check if the string matches the specified regular expression using regcomp(3) */
#define CASES_RE(x,flags) } regfree ( &__regex ); if ( __cont || ( \
                              0 == regcomp ( &__regex, x, flags ) && \
                              0 == regexec ( &__regex, __sw, 0, NULL, 0 ) ) ) { \
                                __done = true; __cont = true;

/** Default behaviour */
#define DEFAULTS    } if ( !__done || __cont ) {

/** Close the switchs */
#define SWITCHS_END } while ( 0 ); regfree(&__regex); }


/***************************************************
 *              Macros
 **************************************************/
// To use with char bf[], not pointers.
#define SET_DECIMAL_STRING(bf, value, precision) \
    snprintf((bf), sizeof(bf), "%.*f", (precision), (double)(value))

#define SET_LONG_DECIMAL_STRING(bf, value, precision) \
    snprintf((bf), sizeof(bf), "%.*Lf", (precision), (long double)(value))

#define SET_STRING(bf, value) \
    snprintf((bf), sizeof(bf), "%s", (value))

static inline BOOL empty_string(const char *str)
{
    return (str && *str)?0:1;
}

/***************************************************
 *              Prototypes
 **************************************************/
/**rst**

.. function:: char *get_parameter(char *s, char **save_ptr)

   Extract parameter: delimited by blanks (\b\t) or quotes ('' "")
   The string is modified (nulls inserted)!

**rst**/
PUBLIC char *get_parameter(char *s, char **save_ptr);

PUBLIC BOOL char_in_chars(const char *chars, char c);

/**rst**

.. function:: char *get_parameter2(char *s, char *f, char **save_ptr)

   Extract parameter: delimited by any char in f
   The string is modified (nulls inserted)!

**rst**/
PUBLIC char *get_parameter2(char *s, char *f, char **save_ptr);


/**rst**

.. function:: char *get_key_value_parameter(char *s, char **key, char **save_ptr)

   Extract key=value or key='this value' parameter
   Return the value, the key in `key`
   The string is modified (nulls inserted)!

**rst**/
PUBLIC char *get_key_value_parameter(char *s, char **key, char **save_ptr);

/**rst**

.. function:: char *strtoupper(char* s)

   Convert string to upper case

**rst**/
PUBLIC char *strtoupper(char* s);

/**rst**

.. function:: char *strntoupper(char* s, size_t n)

   Convert n bytes of string to upper case

**rst**/
PUBLIC char *strntoupper(char* s, size_t n);

/**rst**

.. function:: char *strtolower(char* s)

   Convert string to lower case

**rst**/
PUBLIC char *strtolower(char* s);

/**rst**

.. function:: char *strntolower(char* s, size_t n)

   Convert n bytes of string to lower case

**rst**/
PUBLIC char *strntolower(char* s, size_t n);


/**rst**

.. function:: void delete_right_blanks(char *s)

   Elimina blancos a la derecha. (Espacios, tabuladores, CR's  o LF's)

**rst**/
PUBLIC void delete_right_blanks(char *s);

/**rst**

.. function:: char *delete_right_char(char *s, char x)

*    Elimina el caracter(s) 'x' a la derecha.

**rst**/
PUBLIC char *delete_right_char(char *s, char x);


/**rst**

.. function:: char *delete_left_char(char *s, char x)

    Elimina el caracter 'x' a la izquierda.

**rst**/
PUBLIC char *delete_left_char(char *s, char x);


/**rst**

.. function:: void delete_left_blanks(char *s)

   Elimina blancos a la izquierda. (Espacios, tabuladores, CR's  o LF's)

**rst**/
PUBLIC void delete_left_blanks(char *s);

/**rst**

.. function:: void left_justify(char *s)

   Justifica a la izquierda eliminado blancos a la derecha

**rst**/
PUBLIC void left_justify(char *s);

/**rst**

.. function:: char * translate_string(char *to, int tolen, const char *from, const char *mk_to, const char *mk_from)

   Translate characters from the string 'from' to the string 'to'.
   Usage:
         char * translate_string(char *to,
                                 char *from,
                                 char *mk_to,
                                 char *mk_from);
   Description:
         Move characters from 'from' to 'to' using the masks
   Return:
         A pointer to string 'to'.

**rst**/
PUBLIC char * translate_string(char *to, int tolen, const char *from, const char *mk_to, const char *mk_from);

/**rst**

.. function:: int change_char(char *s, char old_c, char new_c)

   Cambia el caracter old_d por new_c. Retorna los caracteres cambiados

**rst**/
PUBLIC int change_char(char *s, char old_c, char new_c);

/**rst**
    Elimina el caracter c del string. Retorna los caracteres eliminados
**rst**/
PUBLIC int remove_char(char *s, char c);

/**rst**

.. function:: int count_char(char *s, char c)

   Cuenta cuantos caracteres de 'c' hay en 's'

**rst**/
PUBLIC int count_char(const char *s, char c);

/**rst**

.. function:: const char *lstrip(const char* s, size_t str_size, char c)

   return pointer to string position skipping all leading `c` chars

**rst**/
PUBLIC const char *lstrip(const char* s, size_t str_size, char c);

/**rst**

.. function:: const char *lnstrip(const char* s, size_t str_size, char c, int *n)

   return pointer to string position skipping `n` `c` chars

**rst**/
PUBLIC const char *lnstrip(const char* s, size_t str_size, char c, int *n);

/**rst**

.. function:: str_in_list(const char **list, const char *str, BOOL ignore_case)

   Return TRUE if str is in string list (last string int list must be null).

**rst**/
PUBLIC BOOL str_in_list(const char **list, const char *str, BOOL ignore_case);

/**rst**
    Return idx of str in string list. Based in 1. Return 0 if not found.
**rst**/
PUBLIC int idx_in_list(const char **list, const char *str, BOOL ignore_case);

/**rst**

.. function:: BOOL int_in_str_list(const char **list, int i);

   Return TRUE if int i is in string list (last string int list must be null).

**rst**/
PUBLIC BOOL int_in_str_list(const char **list, int i);

/**rst**
    Find String
    buffer          -> string en el que buscar 's'.
    buffer_len      -> longitud de 'buffer'
    s            -> string a buscar
    s_len        -> longitud de 's'
    Retorna un pointer al string encontrado, o NULL si no lo ha encontrado
**rst**/
PUBLIC char * mem_find(const char *buffer, int buffer_len, const char *s, int s_len);

/**rst**
    Encuentra un char, n veces
    buffer          -> string en el que buscar 'c'.#ifdef __cplusplus
    buffer_len      -> longitud de 'buffer'}
    c            -> char a buscar#endif
    n            -> veces que se busca c
    Retorna un pointer al string encontrado, o NULL si no lo ha encontrado#endif
**rst**/
PUBLIC char * mem_find_nchar(char *buffer, int buffer_len, char c, int n);

/**rst**
    Count number of occurrences of a given subdata in a data
    buffer          -> data en el que buscar 's'.
    buffer_len      -> longitud de 'data'
    s            -> subdata a buscar
    s_len        -> longitud de 's'
    Return number of occurrences
**rst**/
PUBLIC int mem_counter(const char *buffer, int buffer_len, const char *s, int s_len);

/**rst**
    Return TRUE if all characters (not empty) are numbers
**rst**/
PUBLIC BOOL all_numbers(const char* s);


#ifdef __cplusplus
}
#endif

#endif

