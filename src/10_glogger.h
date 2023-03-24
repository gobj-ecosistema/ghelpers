/****************************************************************************
 *              GLOGGER.H
 *              Copyright (c) 1996-2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <jansson.h>

/*
 *  Dependencies
 */
#include "00_asn1_snmp.h"
#include "00_gtypes.h"
#include "01_cnv_hex.h"
#include "01_gstrings.h"
#include "02_dl_list.h"
#include "02_time_helper.h"
#include "02_json_buffer.h"
#include "03_json_config.h"

// Handlers included in ghelpers
#include "02_stdout.h"              // "stdout"
#include "03_output_udp_client.h"   // "udp"
#include "03_rotatory.h"            // "file"

#ifdef __cplusplus
extern "C"{
#endif

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

/*****************************************************************
 *     Constants
 *****************************************************************/
/*
 *  Msgid's used by ghelpers
 */
// Error's MSGSETs
#define MSGSET_PARAMETER_ERROR              "Parameter Error"
#define MSGSET_MEMORY_ERROR                 "No memory"
#define MSGSET_INTERNAL_ERROR               "Internal Error"
#define MSGSET_SYSTEM_ERROR                 "OS Error"
#define MSGSET_XML_ERROR                    "XML Error"
#define MSGSET_JSON_ERROR                   "Jansson Error"
#define MSGSET_LIBUV_ERROR                  "Libuv Error"
#define MSGSET_OAUTH_ERROR                  "OAuth Error"
#define MSGSET_TRANGER_ERROR                "Tranger Error"
#define MSGSET_TREEDB_ERROR                 "TreeDb Error"
#define MSGSET_MSG2DB_ERROR                 "Msg2Db Error"

#define MSGSET_QUEUE_ALARM                  "Queue Alarm"

// Info/Debug MSGSETs
#define MSGSET_STATISTICS                   "Statistics"
#define MSGSET_STARTUP                      "Startup"
#define MSGSET_CREATION_DELETION_GBUFFERS   "Creation Deletion GBuffers"
#define MSGSET_DAEMON                       "Daemon"
#define MSGSET_INFO                         "Info"

/*
 *  Value for "gobj" lumberjack field:
 *      a name representing the Yuneta C core code: a well-known mixin world.
 */


/*
    Fields that MUST have a log_...

log_error(0,
    "gobj",         "%s", __FILE__, // or __FUNCTION__
    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
    "msg",          "%s", "Inserting item with PREVIOUS LINKS",
    ...
    NULL
);
log_error(0,
    "gobj",         "%s", gobj_full_name(gobj),
    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
    "msg",          "%s", "Inserting item with PREVIOUS LINKS",
    ...
    NULL
);

*/

/*
 *  Options for handlers
 */
typedef enum {
    LOG_HND_OPT_ALERT           = 0x0001,
    LOG_HND_OPT_CRITICAL        = 0x0002,
    LOG_HND_OPT_ERROR           = 0x0004,
    LOG_HND_OPT_WARNING         = 0x0008,
    LOG_HND_OPT_INFO            = 0x0010,
    LOG_HND_OPT_DEBUG           = 0x0020,
    LOG_HND_OPT_AUDIT           = 0x0040,
    LOG_HND_OPT_MONITOR         = 0x0080,

    LOG_HND_OPT_NODISCOVER      = 0x0100,
    LOG_HND_OPT_NOTIME          = 0x0800,

    LOG_HND_OPT_TRACE_STACK     = 0x1000,
    LOG_HND_OPT_BEATIFUL_JSON   = 0x2000,
    LOG_HND_OPT_DEEP_TRACE      = 0x8000,
} log_handler_opt_t;

#define LOG_OPT_UP_ERROR \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR)

#define LOG_OPT_UP_WARNING \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR|LOG_HND_OPT_WARNING)

#define LOG_OPT_UP_INFO \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR|LOG_HND_OPT_WARNING|LOG_HND_OPT_INFO)

#define LOG_OPT_LOGGER \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR|LOG_HND_OPT_WARNING|LOG_HND_OPT_INFO|LOG_HND_OPT_DEBUG)

#define LOG_OPT_ALL (LOG_OPT_LOGGER|LOG_HND_OPT_AUDIT|LOG_HND_OPT_MONITOR)

/*
 *  Values for log_*() functions
 */
typedef enum {
    LOG_NONE                = 0x0001,   /* Backward compatibility, only log */
    LOG_OPT_EXIT_ZERO       = 0x0002,   /* Exit(0) after log the error or critical */
    LOG_OPT_EXIT_NEGATIVE   = 0x0004,   /* Exit(-1) after log the error or critical */
    LOG_OPT_ABORT           = 0x0008,   /* abort() after log the error or critical */
    LOG_OPT_TRACE_STACK     = 0x0010,   /* Trace stack */
    LOG_DUMP_INPUT          = 0x0020,   /* used in log_dump */
    LOG_DUMP_OUTPUT         = 0x0040,   /* used in log_dump */
} log_opt_t;


/*
 *  Protypes for handlers
 */
/*
 *  HACK Return of callback:
 *      0 continue with next loghandler
 *      -1 break, don't continue with next handlers
 */
typedef void   (*loghandler_close_fn_t)(void *h);
typedef int    (*loghandler_write_fn_t)(void *h, int priority, const char *bf, size_t len);
typedef int    (*loghandler_fwrite_fn_t)(void *h, int priority, const char *format, ...);

typedef void   (*inform_cb_t)(int priority, uint32_t count, void *user_data);


/*****************************************************************
 *     Prototypes
 *****************************************************************/

//TODO modo produccion: impide mas de x logs msg/seg (1000 msg/seg podría estar bien)
//     modo test: logs sin limite

/*
 *  Setup log system
 */
PUBLIC int log_startup(
    const char *app_name,           // application name
    const char *app_version,        // applicacion version
    const char *executable          // executable program, to can trace stack
);
PUBLIC void log_end(void);

PUBLIC int log_set_process_name(const char *process_name);

PUBLIC int log_set_inform_cb(
    inform_cb_t inform_cb,
    void * inform_cb_user_data
);

PUBLIC int log_register_handler(
    const char *handler_type,   // already registered: "stdout", "file", "udp"
    loghandler_close_fn_t close_fn,
    loghandler_write_fn_t write_fn,  // Used by all minus described below in fwrite_fn
    loghandler_fwrite_fn_t fwrite_fn // Used by trace_hex_msg(),log_debug_dump(),log_debug_json() and LOG_OPT_TRACE_STACK option.
);

PUBLIC int log_add_handler(
    const char *handler_name,
    const char *handler_type,   // one of types registered
    log_handler_opt_t handler_options,  // default 0 = LOG_OPT_LOGGER
    void *h     // HACK WARNING don't close directly this handler. It's made with log_del_handler().
);
PUBLIC int log_del_handler(const char* handler_name); // Return # handlers deleted.
PUBLIC json_t *log_list_handlers(void);
PUBLIC BOOL log_exist_handler(const char *handler_name);

PUBLIC void _log_bf(int priority, log_opt_t opt, const char *bf, int len);

PUBLIC void log_clear_counters(void);

PUBLIC const char *log_last_message(void);

/*
 *  Main Log functions.
 */
PUBLIC void log_alert(log_opt_t opt, ...);
PUBLIC void log_critical(log_opt_t opt, ...);
PUBLIC void log_error(log_opt_t opt, ...);
PUBLIC void log_warning(log_opt_t opt, ...);
PUBLIC void log_info(log_opt_t opt, ...);
PUBLIC void log_debug(log_opt_t opt, ...);

/*
 *  LOG_INFO functions.
 */
PUBLIC int info_msg(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)));

PUBLIC int info_msg0(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2))); // without date

/*
 *  LOG_DEBUG functions.
 */
PUBLIC int trace_msg(const char *fmt, ... ) JANSSON_ATTRS((format(printf, 1, 2)));

PUBLIC int trace_msg0(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)));

PUBLIC void trace_hex_msg(
    const char *label,
    const char *bf,
    size_t len
);
PUBLIC void log_debug_vprintf(  /* final lf \n is not needed */
    const char *info,
    const char *fmt,
    va_list ap
) JANSSON_ATTRS((format(printf, 2, 0)));

PUBLIC void log_debug_printf(  /* final lf \n is not needed */
    const char *info,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 2, 3)));

PUBLIC void log_debug_bf(
    log_opt_t opt,
    const char *bf,
    size_t len,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 4, 5)));

PUBLIC void log_debug_vdump(
    log_opt_t opt,
    const char *bf,
    size_t len,
    const char *fmt,        // Not null will display date, bytes, etc
    va_list ap
) JANSSON_ATTRS((format(printf, 4, 0)));

PUBLIC void log_debug_dump(
    log_opt_t opt,
    const char *bf,
    size_t len,
    const char *fmt,        // Not null will display date, bytes, etc
    ...
) JANSSON_ATTRS((format(printf, 4, 5)));

PUBLIC void log_debug_json(
    log_opt_t opt,
    json_t *jn,  // not owned
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 3, 4)));

/*
 *  LOG_MONITOR functions.
 */
PUBLIC void log_monitor(
    log_opt_t opt,
    ...
);

/**rst**
    Set real precision
    Return the previous precision
**rst**/
PUBLIC int set_real_precision(int precision);
PUBLIC int get_real_precision(void);


#ifdef __cplusplus
}
#endif
