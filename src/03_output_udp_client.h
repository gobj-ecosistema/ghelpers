/****************************************************************************
 *          OUTPUT_UDP_CLIENT.H
 *          Only Output client upd
 *
 *          Copyright (c) 2014, 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/

#ifndef __OUTPUT_UDP_CLIENT_H
#define __OUTPUT_UDP_CLIENT_H 1

#include <stdio.h>
#include <stdarg.h>

/*
 *  Dependencies
 */
#include "01_print_error.h"
#include "02_urls.h"
#include "02_dl_list.h"

#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Constants
 *****************************************************************/
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

typedef enum { // All formats are terminator null!
    /*
     *  In OUTPUT_FORMAT_YUNETA the ``bf`` to write must be an json dict or json string.
     *  Yuneta format
     *      "%c%08X:%s",
     *          '0' + priority,
     *          uc->counter++,
     *          bf
     *
     */
    OUTPUT_FORMAT_YUNETA = 0,

    /*
     *  In OUTPUT_FORMAT_LUMBERJACK the ``bf`` to write must be an json dict.
     *  See lumberjack https://fedorahosted.org/lumberjack/ .
     *  Otherwise it's up to you.
     */
    OUTPUT_FORMAT_LUMBERJACK,

    /*
     *  Output to system's syslog
     */
    OUTPUT_FORMAT_SYSLOG,

    /*
     *  udpc_write() add only the terminator null
     */
    OUTPUT_FORMAT_RAW,      // WARNING with hardcore references

    OUTPUT_OLD_FORMAT_YUNETA,

    OUTPUT_LAST_ENTRY
} ouput_format_t;

/*****************************************************************
 *     Structures
 *****************************************************************/
typedef void * udpc_t;

/*********************************************************************
 *      Prototypes
 *********************************************************************/
PUBLIC int udpc_start_up(
    const char *process_name
);
PUBLIC void udpc_end(void);                             // close all

// Return NULL on error
PUBLIC udpc_t udpc_open(
    const char *url,
    const char *bindip,
    size_t bf_size,                 // 0 = default 256K
    size_t udp_frame_size,          // 0 = default 1500
    ouput_format_t output_format,   // 0 = default OUTPUT_FORMAT_YUNETA
    BOOL exit_on_fail
);
PUBLIC void udpc_close(udpc_t udpc);

/*
    Esta función está pensada para enviar logs con json, es decir, strings.
    El string puede ser hasta 64K pero se envia troceado en trozos de udp_frame_size con un nulo al final.
    EL string tiene que incluir el nulo, y si no es así, se añade internamente.
    El receptor distingue los records (strings) por el nulo finalizador.
    64k puede parecer mucho, pero en los gobj-tree grandes se puede superar si piden una view.

    Ojo que el socket es síncrono.
    Eso quiere decir que escrituras masivas consumen mucho tiempo
    (retorna cuando ha concluido de escribir en la red
    y se consciente que bloqueas el yuno durante ese tiempo.
    Si solo tienes un core entonces bloqueas toda la máquina.

    Max len ``bf_size`` bytes
    Return -1 if error
 */
PUBLIC int udpc_write(udpc_t udpc, int priority, const char *bf, size_t len);
PUBLIC int udpc_fwrite(udpc_t udpc, int priority, const char *format, ...);

//TODO funcion de carga: # udpcs

#ifdef __cplusplus
}
#endif


#endif
