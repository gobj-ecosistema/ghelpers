/***********************************************************************
 *              CONVERT HEXADECIMAL library
 *              Copyright (c) 1995 Niyamaka.
 *              All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdlib.h>
#include "01_cnv_hex.h"

/***********************************************************************
 *     Prototypes
 ***********************************************************************/

/*********************************************************************
 *        Data
 *********************************************************************/
PRIVATE char base16_decoding_table1[256] = {
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};


static const char tbhexa[]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

/***********************************************************************
 *  byte_to_strhex - sprintf %02X
 *  Usage:
 *        char *byte_to_strhex(char *strh, uint8_t w)
 *  Description:
 *        Print in 'strh' the Hexadecimal ASCII representation of 'w'
 *        Equivalent to sprintf %02X SIN EL NULO
 *        OJO que hay f() que usan esta caracteristica
 *  Return:
 *        Pointer to endp
 ***********************************************************************/
PRIVATE char * byte_to_strhex(char *s, uint8_t w)
{
    *s = tbhexa[ ((w >> 4) & 0x0f) ];
    s++;
    *s = tbhexa[ ((w) & 0x0f) ];
    s++;
    return s;
}

/***********************************************************************
 *  hex2bin
 *  return bf
 ***********************************************************************/
char *hex2bin(char *bf, int bfsize, const char *hex, size_t hex_len, size_t *out_len)
{
    size_t i;
    char cur;
    char val;
    for (i = 0; i < hex_len; i++) {
        cur = hex[i];
        val = base16_decoding_table1[(int)cur];
        /* even characters are the first half, odd characters the second half
         * of the current output byte */
        if(val < 0) {
            break; // Found non-hex, break
        }
        if(i/2 >= bfsize) {
            break;
        }
        if (i%2 == 0) {
            bf[i/2] = val << 4;
        } else {
            bf[i/2] |= val;
        }
    }
    if(out_len) {
        *out_len = i/2;
    }
    return bf;
}

/***********************************************************************
 *  bin2hex
 *  return bf
 ***********************************************************************/
char *bin2hex(char *bf, int bfsize, const char *bin, size_t bin_len)
{
    char *p;
    size_t i, j;

    p = bf;
    for(i=j=0; i<bin_len && j < (bfsize-2); i++, j+=2) {
        p = byte_to_strhex(p, *bin++);
    }
    *p = 0; // final null
    return bf;
}

/***********************************************************************
 *
 ***********************************************************************/
BOOL all_00(uint8_t *bf, int bfsize)
{
    for(int i=0; i<bfsize; i++) {
        if(bf[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

/***********************************************************************
 *
 ***********************************************************************/
BOOL all_ff(uint8_t *bf, int bfsize)
{
    for(int i=0; i<bfsize; i++) {
        if(bf[i]!=0xff) {
            return FALSE;
        }
    }
    return TRUE;
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *  nivel 1 -> solo en hexa
 *  nivel 2 -> en hexa y en asci
 *  nivel 3 -> en hexa y en asci y con contador (indice)
 ***********************************************************************/
PRIVATE void _tdump(const char *prefix, const char *s, size_t len, view_fn_t view, int nivel)
{
    static char bf[80+1];
    static char asci[40+1];
    char *p;
    size_t i, j;

    if(!prefix)
        prefix = (char *)"";
    p = bf;
    for(i=j=0; i<len; i++) {
        asci[j] = (*s<' ')? '.': *s;
        if(asci[j] == '%')
            asci[j] = '.';
        j++;
        p = byte_to_strhex(p, *s++);
        *p++ = ' ';
        if(j == 16) {
            *p++ = '\0';
            asci[j] = '\0';

            if(nivel==1) {
                view("%s%s\n", prefix, bf);

            } else if(nivel==2) {
                view("%s%s  %s\n", prefix, bf, asci);

            } else {
                view("%s%04X: %s  %s\n", prefix, i-15, bf, asci);
            }

            p = bf;
            j = 0;
        }
    }
    if(j) {
        len = 16 - j;
        while(len-- >0) {
            *p++ = ' ';
            *p++ = ' ';
            *p++ = ' ';
        }
        *p++ = '\0';
        asci[j] = '\0';

        if(nivel==1) {
           view("%s%s\n", prefix, bf);

        } else if(nivel==2) {
            view("%s%s  %s\n", prefix, bf, asci);

        } else {
            view("%s%04X: %s  %s\n", prefix, i - j, bf, asci);
        }
    }
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    Contador, hexa, ascii
 ***********************************************************************/
void tdump(const char *s, size_t len, view_fn_t view)
{
    _tdump(0, s,len,view,3);
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    Solo hexa
 ***********************************************************************/
void tdump1(const char *s, size_t len, view_fn_t view)
{
    _tdump(0, s,len,view,1);
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    En hexa, ascii
 ***********************************************************************/
void tdump2(const char *s, size_t len, view_fn_t view)
{
    _tdump(0, s,len,view,2);
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    Contador, hexa, ascii
 ***********************************************************************/
void tdumps(const char *prefix, const char *s, size_t len, view_fn_t view)
{
    _tdump(prefix, s,len,view,3);
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    Solo hexa
 ***********************************************************************/
void tdumps1(const char *prefix, const char *s, size_t len, view_fn_t view)
{
    _tdump(prefix, s,len,view,1);
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    En hexa, ascii
 ***********************************************************************/
void tdumps2(const char *prefix, const char *s, size_t len, view_fn_t view)
{
    _tdump(prefix, s,len,view,2);
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    Contador, hexa, ascii
 ***********************************************************************/
void tdumpsu(void *user_data, int user_data2, const char *prefix, const char *s, size_t len, view_fn2_t view)
{
    char hexa[80+1];
    char asci[40+1];
    char *p;
    size_t i, j;

    if(!prefix) {
        prefix = (char *)"";
    }
    int line_size = strlen(prefix) + 76 ; // index + hex_size + ascii_size + \n
    int total_buffer = (len/16 + 1)* line_size;
    char *buffer = malloc(total_buffer);
    if(!buffer) {
        // log error?
        return;
    }
    memset(buffer, 0, total_buffer);
    char *pbuf = buffer;
    p = hexa;
    for(i=j=0; i<len; i++) {
        asci[j] = (*s<' ')? '.': *s;
        if(asci[j] == '%')
            asci[j] = '.';
        j++;
        p = byte_to_strhex(p, *s++);
        *p++ = ' ';
        if(j == 16) {
            *p++ = '\0';
            asci[j] = '\0';

            int written = snprintf(pbuf, total_buffer, "%s%04X: %s  %s\n", prefix, (int)i-15, hexa, asci);
            if(written > 0) {
                pbuf += written;
                total_buffer -= written;
                if(total_buffer < 0) {
                    print_error(0, "ERROR", "BAD buffer len");
                }
            } else {
                break;
            }

            p = hexa;
            j = 0;
        }
    }
    if(j) {
        len = 16 - j;
        while(len-- >0) {
            *p++ = ' ';
            *p++ = ' ';
            *p++ = ' ';
        }
        *p++ = '\0';
        asci[j] = '\0';

        int written = snprintf(pbuf, total_buffer, "%s%04X: %s  %s\n", prefix, (int)(i-j), hexa, asci);
        if(written > 0) {
            pbuf += written;
            total_buffer -= written;
        }
    }
    view(user_data, user_data2, "\n%s", buffer);
    free(buffer);
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    hexa
 ***********************************************************************/
void tdumpsu2(void *user_data, int user_data2, const char *prefix, const char *s, size_t len, view_fn2_t view)
{
    if(!prefix) {
        prefix = "";
    }
    int total_buffer = strlen(prefix)+4+len*2;
    char *buffer = malloc(total_buffer);
    if(!buffer) {
        // log error?
        return;
    }
    memset(buffer, 0, total_buffer);
    snprintf(buffer, total_buffer, "%s: ", prefix);
    char *p = buffer + strlen(buffer);

    for(int i=0; i<len; i++) {
        p = byte_to_strhex(p, *s++);
    }
    view(user_data, user_data2, "%s", buffer);
    free(buffer);
}
