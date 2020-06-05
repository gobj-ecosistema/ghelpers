/****************************************************************************
 *              GLOGGER.C
 *              Copyright (c) 1996-2015 Niyamaka.
 *              All Rights Reserved.
 *
 *  WARNING here json_buffer is used,
 *  jansson cannot be used because it can use gbmem
 *
 ****************************************************************************/
#define _GNU_SOURCE 1   /* */
#define _DEFAULT_SOURCE     /* to get gethostname() */

#include <sys/types.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <pwd.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <uv.h>
#include "10_glogger.h"

/*****************************************************************
 *          Constants
 *****************************************************************/
#define MAX_HANDLER_TYPES 10

/*****************************************************************
 *          Structures
 *****************************************************************/
typedef struct {
    char handler_type[32+1];
    loghandler_close_fn_t close_fn;
    loghandler_write_fn_t write_fn;
    loghandler_fwrite_fn_t fwrite_fn;
} hregister_t;

typedef struct {
    DL_ITEM_FIELDS

    char *handler_name;
    log_handler_opt_t handler_options;
    hregister_t *hr;
    void *h;
} log_handler_t;

/*****************************************************************
 *          Data
 *****************************************************************/
PRIVATE char __app_name__[64] = {0};
PRIVATE char __app_version__[64] = {0};
PRIVATE char __executable__[512] = {0};
PRIVATE inform_cb_t __inform_cb__ = 0;
PRIVATE void *__inform_cb_user_data__ = 0;
PRIVATE char last_message[4*1024+1];
PRIVATE uint32_t __alert_count__ = 0;
PRIVATE uint32_t __critical_count__ = 0;
PRIVATE uint32_t __error_count__ = 0;
PRIVATE uint32_t __warning_count__ = 0;
PRIVATE uint32_t __info_count__ = 0;
PRIVATE uint32_t __debug_count__ = 0;

#ifdef USE_MUTEX
PRIVATE uv_mutex_t mutex_log;
#endif

PRIVATE hgen_t hgen;
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE char __initialized__ = 0;
PRIVATE dl_list_t dl_clients;
PRIVATE int max_hregister = 0;
PRIVATE hregister_t hregister[MAX_HANDLER_TYPES+1];

/*****************************************************************
 *          Prototypes
 *****************************************************************/
extern void jsonp_free(void *ptr);
PRIVATE void _log_jnbf(int priority, log_opt_t opt, va_list ap);
PRIVATE void show_backtrace(loghandler_fwrite_fn_t fwrite_fn, void* h);
PRIVATE void discover(hgen_t hgen);
PRIVATE void json_append(hgen_t hgen, ...);
PRIVATE void json_vappend(hgen_t hgen, va_list ap);
PRIVATE BOOL must_ignore(log_handler_t *lh, int priority);


/*****************************************************************
 *        Setup system log
 *****************************************************************/
PUBLIC int log_startup(
    const char *app_name,           // application name
    const char *app_version,        // applicacion version
    const char *executable)         // executable program, to can trace stack
{
    if(__initialized__) {
        return -1;
    }
    if(app_name) {
        strncpy(__app_name__ , app_name, sizeof(__app_name__)-1);
    }
    if(app_version) {
        strncpy(__app_version__ , app_version, sizeof(__app_version__)-1);
    }
    if(executable) {
        strncpy(__executable__ , executable, sizeof(__executable__)-1);
    }

#ifdef USE_MUTEX
    if (uv_mutex_init(&mutex_log)) {
        print_error(
            PEF_ABORT,
            "ERROR YUNETA",
            "uv_mutex_init() FAILED"
        );
    }
#endif
    if (!atexit_registered) {
        atexit(log_end);
        atexit_registered = 1;
    }

    if(!hgen) {
        hgen = json_dict();
        if(!hgen) {
            print_error(
                PEF_ABORT,
                "ERROR YUNETA",
                "json_dict(): FAILED"
            );
        }
    }

    /*
     *  Register handlers included
     */
    log_register_handler(
        "stdout",           // handler_name
        0,                  // close_fn
        stdout_write,       // write_fn
        stdout_fwrite       // fwrite_fn
    );
    log_register_handler(
        "file",             // handler_name
        rotatory_close,     // close_fn
        rotatory_write,     // write_fn
        rotatory_fwrite     // fwrite_fn
    );
    log_register_handler(
        "udp",              // handler_name
        udpc_close,         // close_fn
        udpc_write,         // write_fn
        udpc_fwrite         // fwrite_fn
    );

    dl_init(&dl_clients);
    __initialized__ = TRUE;
    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC void log_end(void)
{
    log_handler_t *lh;

    if(!__initialized__) {
        return;
    }

    while((lh=dl_first(&dl_clients))) {
        log_del_handler(lh->handler_name);
    }
    max_hregister = 0;
#ifdef USE_MUTEX
    uv_mutex_destroy(&mutex_log);
#endif
    __initialized__ = FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int log_set_process_name(const char *process_name)
{
    if(process_name) {
        strncpy(__app_name__ , process_name, sizeof(__app_name__)-1);
    }
    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int log_set_inform_cb(
    inform_cb_t inform_cb,
    void * inform_cb_user_data)
{
    __inform_cb__ = inform_cb;
    __inform_cb_user_data__ = inform_cb_user_data;
    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int log_register_handler(
    const char* handler_type,
    loghandler_close_fn_t close_fn,
    loghandler_write_fn_t write_fn,
    loghandler_fwrite_fn_t fwrite_fn)
{
    if(max_hregister >= MAX_HANDLER_TYPES) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "log_register_handler(): REGISTER FULL"
        );
        return -1;
    }
    strncpy(
        hregister[max_hregister].handler_type,
        handler_type,
        sizeof(hregister[0].handler_type) - 1
    );
    hregister[max_hregister].close_fn = close_fn;
    hregister[max_hregister].write_fn = write_fn;
    hregister[max_hregister].fwrite_fn = fwrite_fn;

    max_hregister++;
    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int log_add_handler(
    const char *handler_name,
    const char *handler_type,
    log_handler_opt_t handler_options,
    void *h)
{
    if(!__initialized__) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "log_add_handler(): glogger not initialized"
        );
        return -1;
    }
    if(empty_string(handler_name)) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "log_add_handler(): handler name null"
        );
        return -1;
    }

    /*-------------------------------------*
     *          Find type
     *-------------------------------------*/
    int type;
    for(type = 0; type < max_hregister; type++) {
        if(strcmp(hregister[type].handler_type, handler_type)==0) {
            break;
        }
    }
    if(type == max_hregister) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "log_add_handler(): handler_type %s NOT FOUND",
            handler_type
        );
        return -1;
    }

    /*-------------------------------------*
     *          Alloc memory
     *-------------------------------------*/
    log_handler_t *lh = malloc(sizeof(log_handler_t));
    if(!lh) {
        print_error(
            PEF_ABORT,
            "ERROR YUNETA",
            "log_add_handler(): No MEMORY % bytes",
            sizeof(log_handler_t)
        );
        return -1;
    }
    memset(lh, 0, sizeof(log_handler_t));
    lh->handler_name = strdup(handler_name);
    lh->handler_options = handler_options?handler_options:LOG_OPT_LOGGER;
    lh->hr = &hregister[type];
    lh->h = h;

    /*----------------*
     *  Add to list
     *----------------*/
    return dl_add(&dl_clients, lh);
}

/*****************************************************************
 *  Return handlers deleted.
 *****************************************************************/
PUBLIC int log_del_handler(const char *handler_name)
{
    if(empty_string(handler_name)) {
        return 0;
    }
    int ret = 0;
    log_handler_t *lh = dl_first(&dl_clients);
    while(lh) {
        log_handler_t *next = dl_next(lh);
        if(strcmp(lh->handler_name, handler_name)==0) {
            ret++;
            dl_delete(&dl_clients, lh, 0);
            if(lh->h && lh->hr->close_fn) {
                lh->hr->close_fn(lh->h);
            }
            if(lh->handler_name) {
                free(lh->handler_name);
            }
            free(lh);
        }
        /*
         *  Next
         */
        lh = next;
    }
    return ret;
}

/*****************************************************************
 *  Return list of handlers
 *****************************************************************/
PUBLIC json_t *log_list_handlers(void)
{
    json_t *jn_array = json_array();
    log_handler_t *lh = dl_first(&dl_clients);
    while(lh) {
        json_t *jn_dict = json_object();
        json_array_append_new(jn_array, jn_dict);
        json_object_set_new(jn_dict, "handler_name", json_string(lh->handler_name));
        json_object_set_new(jn_dict, "handler_type", json_string(lh->hr->handler_type));
        json_object_set_new(jn_dict, "handler_options", json_integer(lh->handler_options));

        /*
         *  Next
         */
        lh = dl_next(lh);
    }
    return jn_array;
}

/*****************************************************************
 *      Clear counters
 *****************************************************************/
PUBLIC void log_clear_counters(void)
{
    __debug_count__ = 0;
    __info_count__ = 0;
    __warning_count__ = 0;
    __error_count__ = 0;
    __critical_count__ = 0;
    __alert_count__ = 0;
}

/*****************************************************************
 *      Log alert
 *****************************************************************/
PUBLIC const char *log_last_message(void)
{
    return last_message;
}

/*****************************************************************
 *      Log alert
 *****************************************************************/
PUBLIC void log_alert(log_opt_t opt, ...)
{
    va_list ap;
    int priority = LOG_ALERT;

    __alert_count__++;

    va_start(ap, opt);
    _log_jnbf(priority, opt, ap);
    va_end(ap);

    if(__inform_cb__) {
        __inform_cb__(priority, __alert_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Log critical
 *****************************************************************/
PUBLIC void log_critical(log_opt_t opt, ...)
{
    va_list ap;
    int priority = LOG_CRIT;

    __critical_count__++;

    va_start(ap, opt);
    _log_jnbf(priority, opt, ap);
    va_end(ap);

    if(__inform_cb__) {
        __inform_cb__(priority, __critical_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Log error
 *****************************************************************/
PUBLIC void log_error(log_opt_t opt, ...)
{
    va_list ap;
    int priority = LOG_ERR;

    __error_count__++;

    va_start(ap, opt);
    _log_jnbf(priority, opt, ap);
    va_end(ap);

    if(__inform_cb__) {
        __inform_cb__(priority, __error_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Log warning
 *****************************************************************/
PUBLIC void log_warning(log_opt_t opt, ...)
{
    va_list ap;
    int priority = LOG_WARNING;

    __warning_count__++;

    va_start(ap, opt);
    _log_jnbf(priority, opt, ap);
    va_end(ap);

    if(__inform_cb__) {
        __inform_cb__(priority, __warning_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Log info
 *****************************************************************/
PUBLIC void log_info(log_opt_t opt, ...)
{
    va_list ap;
    int priority = LOG_INFO;

    __info_count__++;

    va_start(ap, opt);
    _log_jnbf(priority, opt, ap);
    va_end(ap);

    if(__inform_cb__) {
        __inform_cb__(priority, __info_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Log debug
 *****************************************************************/
PUBLIC void log_debug(log_opt_t opt, ...)
{
    va_list ap;
    int priority = LOG_DEBUG;

    __info_count__++;

    va_start(ap, opt);
    _log_jnbf(priority, opt, ap);
    va_end(ap);

    if(__inform_cb__) {
        __inform_cb__(priority, __info_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Info printf
 *****************************************************************/
PUBLIC int info_msg0(const char *fmt, ...)
{
    int priority = LOG_INFO;
    log_opt_t opt = 0;
    va_list ap;
    char temp[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(temp, sizeof(temp), fmt, ap);
    _log_bf(priority, opt, temp, strlen(temp));
    va_end(ap);
    if(__inform_cb__) {
        __inform_cb__(priority, __debug_count__, __inform_cb_user_data__);
    }
    return 0;
}

/*****************************************************************
 *      Info printf
 *****************************************************************/
PUBLIC int info_msg(const char *fmt, ...)
{
    int priority = LOG_INFO;
    log_opt_t opt = 0;
    va_list ap;
    char dtemp[90];
    char temp[BUFSIZ];

    current_timestamp(dtemp, sizeof(dtemp));
    snprintf(temp, sizeof(temp),
        "%s - ",
        dtemp
    );
    int len = strlen(temp);
    va_start(ap, fmt);
    vsnprintf(temp+len, sizeof(temp)-len, fmt, ap);
    _log_bf(priority, opt, temp, strlen(temp));
    va_end(ap);
    if(__inform_cb__) {
        __inform_cb__(priority, __debug_count__, __inform_cb_user_data__);
    }
    return 0;
}

/*****************************************************************
 *      Debug printf
 *****************************************************************/
PUBLIC int trace_msg(const char *fmt, ...)
{
    int priority = LOG_DEBUG;
    log_opt_t opt = 0;
    va_list ap;
    char dtemp[90];
    char temp[BUFSIZ];

    current_timestamp(dtemp, sizeof(dtemp));
    snprintf(temp, sizeof(temp),
        "%s - ",
        dtemp
    );
    int len = strlen(temp);
    va_start(ap, fmt);
    vsnprintf(temp+len, sizeof(temp)-len, fmt, ap);
    _log_bf(priority, opt, temp, strlen(temp));
    va_end(ap);
    if(__inform_cb__) {
        __inform_cb__(priority, __debug_count__, __inform_cb_user_data__);
    }
    return 0;
}

/*****************************************************************
 *      Debug printf
 *****************************************************************/
PUBLIC int trace_msg0(const char *fmt, ...)
{
    int priority = LOG_DEBUG;
    log_opt_t opt = 0;
    va_list ap;
    char temp[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(temp, sizeof(temp), fmt, ap);
    _log_bf(priority, opt, temp, strlen(temp));
    va_end(ap);
    if(__inform_cb__) {
        __inform_cb__(priority, __debug_count__, __inform_cb_user_data__);
    }
    return 0;
}

/*****************************************************************
 *      Trace a message in hexa
 *****************************************************************/
PUBLIC void trace_hex_msg(
    const char *label,
    const char *bf,
    size_t len)
{
    if(!__initialized__) {
        return;
    }
    if(!bf) {
        return;
    }

    int priority = LOG_DEBUG;
#ifdef USE_MUTEX
    uv_mutex_lock(&mutex_log);
#endif
    log_handler_t *lh = dl_first(&dl_clients);
    while(lh) {
        if(must_ignore(lh, priority)) {
            /*
             *  Next
             */
            lh = dl_next(lh);
            continue;
        }
        if(lh->hr->fwrite_fn) {
            tdumpsu2(lh->h, priority, label, bf, len, lh->hr->fwrite_fn);
        }

        /*
         *  Next
         */
        lh = dl_next(lh);
    }
#ifdef USE_MUTEX
    uv_mutex_unlock(&mutex_log);
#endif
    if(__inform_cb__) {
        __inform_cb__(priority, __debug_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Debug printf
 *****************************************************************/
PUBLIC void log_debug_vprintf(const char *info, const char *fmt, va_list ap)
{
    int priority = LOG_DEBUG;
    log_opt_t opt = 0;
    char dtemp[90];
    char temp[BUFSIZ];

    if(info) {
        current_timestamp(dtemp, sizeof(dtemp));
        if(*info) {
            snprintf(temp, sizeof(temp),
                "%s - %s - ",
                dtemp,
                info
            );
        } else {
            snprintf(temp, sizeof(temp),
                "%s - ",
                dtemp
            );
        }
    } else {
        temp[0] = 0;
    }
    int len = strlen(temp);

    va_list aq;
    va_copy(aq, ap);

    vsnprintf(temp+len, sizeof(temp)-len, fmt, aq);
    _log_bf(priority, opt, temp, strlen(temp));


    if(__inform_cb__) {
        __inform_cb__(priority, __debug_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Debug printf
 *****************************************************************/
PUBLIC void log_debug_printf(const char *info, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_debug_vprintf(info, fmt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Debug print buffer
 *****************************************************************/
PUBLIC void log_debug_bf(
    log_opt_t opt,
    const char *bf,
    size_t len,
    const char *fmt,
    ...
)
{
    int priority = LOG_DEBUG;
    va_list ap;
    char dtemp[90];
    char temp[4*1024];

    if(!bf) {
        return;
    }
    __debug_count__++;

    if(fmt) {
        current_timestamp(dtemp, sizeof(dtemp));
        if(*fmt) {
            snprintf(temp, sizeof(temp),
                "%s - ",
                dtemp
            );
        } else {
            snprintf(temp, sizeof(temp),
                "%s",
                dtemp
            );
        }
        if(*fmt) {
            int l = strlen(temp);
            va_start(ap, fmt);
            vsnprintf(temp+l, sizeof(temp)-l, fmt, ap);
            _log_bf(priority, opt, temp, strlen(temp));
            va_end(ap);

        }
    } else {
        temp[0] = 0;
    }
    if(temp[0]) {
        _log_bf(priority, opt, temp, strlen(temp));
    }
    _log_bf(priority, opt, bf, len);

    if(__inform_cb__) {
        __inform_cb__(priority, __debug_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Debug dump hexa
 *****************************************************************/
PUBLIC void log_debug_dump(
    log_opt_t opt,
    const char *bf,
    size_t len,
    const char *fmt,
    ...
)
{
    int priority = LOG_DEBUG;
    va_list ap;
    char dtemp[90];
    char temp[4*1024];

    if(!__initialized__) {
        return;
    }
    if(!bf) {
        return;
    }
    __debug_count__++;

    const char *direction="";
    if(opt & LOG_DUMP_INPUT) {
        direction = "FROM";
    }
    if(opt & LOG_DUMP_OUTPUT) {
        direction = "TO";
    }

    if(fmt) {
        current_timestamp(dtemp, sizeof(dtemp));
        if(*fmt) {
            snprintf(temp, sizeof(temp),
                "%s - Data (%zu bytes) %s ",
                dtemp,
                len,
                direction
            );
        } else {
            snprintf(temp, sizeof(temp),
                "%s - Data (%zu bytes)",
                dtemp,
                len
            );
        }
        if(*fmt) {
            int l = strlen(temp);
            va_start(ap, fmt);
            vsnprintf(temp+l, sizeof(temp)-l, fmt, ap);
            va_end(ap);

        }
    } else {
        temp[0] = 0;
    }

#ifdef USE_MUTEX
    uv_mutex_lock(&mutex_log);
#endif
    log_handler_t *lh = dl_first(&dl_clients);
    while(lh) {
        if(must_ignore(lh, priority)) {
            /*
             *  Next
             */
            lh = dl_next(lh);
            continue;
        }

        if(temp[0] && lh->hr->write_fn) {
            lh->hr->write_fn(lh->h, priority, temp, strlen(temp));
        }
        if(lh->hr->fwrite_fn) {
            tdumpsu(lh->h, priority, 0, bf, len, lh->hr->fwrite_fn);
        }

        /*
         *  Next
         */
        lh = dl_next(lh);
    }
#ifdef USE_MUTEX
    uv_mutex_unlock(&mutex_log);
#endif
    if(__inform_cb__) {
        __inform_cb__(priority, __debug_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Debug json
 *****************************************************************/
PUBLIC void log_debug_json(
    log_opt_t opt,
    json_t *jn,  // not owned
    const char *fmt,
    ...
)
{
    extern void jsonp_free(void *ptr);
    int priority = LOG_DEBUG;
    va_list ap;
    char dtemp[90];
    char temp[4*1024];

    if(!__initialized__) {
        return;
    }

    if(!jn) {
        return; // silence
    }
    __debug_count__++;

    const char *direction="";
    if(opt & LOG_DUMP_INPUT) {
        direction = "FROM";
    }
    if(opt & LOG_DUMP_OUTPUT) {
        direction = "TO";
    }

    char *s = json_dumps(
        jn,
        JSON_ENCODE_ANY|JSON_INDENT(4)|JSON_REAL_PRECISION(get_real_precision())
    );
    BOOL bad_json = FALSE;
    if(!s) {
        s = "BAD JSON or NULL";
        bad_json = TRUE;
    }
    size_t len = s?strlen(s):0;

    if(fmt) {
        current_timestamp(dtemp, sizeof(dtemp));
        if(*fmt) {
            snprintf(temp, sizeof(temp),
                "%s - Data (%zu bytes) %s ",
                dtemp,
                len,
                direction
            );
        } else {
            snprintf(temp, sizeof(temp),
                "%s - Data (%zu bytes)",
                dtemp,
                len
            );
        }
        if(*fmt) {
            int l = strlen(temp);
            va_start(ap, fmt);
            vsnprintf(temp+l, sizeof(temp)-l, fmt, ap);
            va_end(ap);

        }
    } else {
        temp[0] = 0;
    }

#ifdef USE_MUTEX
    uv_mutex_lock(&mutex_log);
#endif
    log_handler_t *lh = dl_first(&dl_clients);
    if(s) {
        while(lh) {
            if(must_ignore(lh, priority)) {
                /*
                 *  Next
                 */
                lh = dl_next(lh);
                continue;
            }
            if(temp[0] && lh->hr->write_fn) {
                lh->hr->write_fn(lh->h, priority, temp, strlen(temp));
            }
            if(lh->hr->fwrite_fn) {
                lh->hr->write_fn(lh->h, priority, s, len);
            }
            if(bad_json) {
                show_backtrace(lh->hr->fwrite_fn, lh->h);
            }

            /*
             *  Next
             */
            lh = dl_next(lh);
        }
        if(!bad_json) {
            jsonp_free(s);
        }
    }
#ifdef USE_MUTEX
    uv_mutex_unlock(&mutex_log);
#endif

    if(__inform_cb__) {
        __inform_cb__(priority, __debug_count__, __inform_cb_user_data__);
    }
}

/*****************************************************************
 *      Monitor json
 *****************************************************************/
PUBLIC void log_monitor(
    log_opt_t opt,
    ...)
{
    va_list ap;
    int priority = LOG_MONITOR;

    va_start(ap, opt);
    _log_jnbf(priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE BOOL must_ignore(log_handler_t *lh, int priority)
{
    BOOL ignore = TRUE;
    log_handler_opt_t handler_options = lh->handler_options;

    switch(priority) {
    case LOG_ALERT:
        if(handler_options & LOG_HND_OPT_ALERT)
            ignore = FALSE;
        break;
    case LOG_CRIT:
        if(handler_options & LOG_HND_OPT_CRITICAL)
            ignore = FALSE;
        break;
    case LOG_ERR:
        if(handler_options & LOG_HND_OPT_ERROR)
            ignore = FALSE;
        break;
    case LOG_WARNING:
        if(handler_options & LOG_HND_OPT_WARNING)
            ignore = FALSE;
        break;
    case LOG_INFO:
        if(handler_options & LOG_HND_OPT_INFO)
            ignore = FALSE;
        break;
    case LOG_DEBUG:
        if(handler_options & LOG_HND_OPT_DEBUG)
            ignore = FALSE;
        break;
    case LOG_AUDIT:
        if(handler_options & LOG_HND_OPT_AUDIT)
            ignore = FALSE;
        break;
    case LOG_MONITOR:
        if(handler_options & LOG_HND_OPT_MONITOR)
            ignore = FALSE;
        break;

    default:
        break;
    }
    return ignore;
}

/*****************************************************************
 *      Log data in json format
 *****************************************************************/
PRIVATE void _log_jnbf(int priority, log_opt_t opt, va_list ap)
{
    if(!__initialized__) {
        return;
    }
#ifdef USE_MUTEX
    uv_mutex_lock(&mutex_log);
#endif
    log_handler_t *lh = dl_first(&dl_clients);
    while(lh) {
        if(must_ignore(lh, priority)) {
            /*
             *  Next
             */
            lh = dl_next(lh);
            continue;
        }

        if(lh->hr->write_fn) {
            // TODO with a hgen_dup analiza si es el mismo msg para no repetirlo
            json_reset(hgen, (lh->handler_options & LOG_HND_OPT_BEATIFUL_JSON)?TRUE:FALSE);
            if(!(lh->handler_options & LOG_HND_OPT_NOTIME)) {
                char stamp[90];
                current_timestamp(stamp, sizeof(stamp));
                json_append(hgen,
                    "timestamp", "%s", stamp,
                    NULL
                );
            }
            va_list ap_;
            va_copy(ap_, ap);
            json_vappend(hgen, ap_); // TODO las keys repetidas APARECEN!! cambia el json!!
            va_end(ap_);
            if(priority <= LOG_CRIT || !(lh->handler_options & LOG_HND_OPT_NODISCOVER)) {
                // LOG_EMERG LOG_ALERT LOG_CRIT always use discover()
                discover(hgen);
            }
            char *bf = json_get_buf(hgen);
            int ret = (lh->hr->write_fn)(lh->h, priority, bf, strlen(bf));
            if(ret < 0) { // Handler owns the message
                break;
            }
        }
        if(lh->hr->fwrite_fn) {
            if((opt & (LOG_OPT_TRACE_STACK|LOG_OPT_EXIT_NEGATIVE|LOG_OPT_ABORT)) ||
                    ((lh->handler_options & LOG_HND_OPT_TRACE_STACK) && priority <=LOG_ERR)
                ) {
                show_backtrace(lh->hr->fwrite_fn, lh->h);
            }
        }

        /*
         *  Next
         */
        lh = dl_next(lh);
    }
#ifdef USE_MUTEX
    uv_mutex_unlock(&mutex_log);
#endif
    if(opt & LOG_OPT_EXIT_NEGATIVE) {
        exit(-1);
    }
    if(opt & LOG_OPT_EXIT_ZERO) {
        exit(0);
    }
    if(opt & LOG_OPT_ABORT) {
        abort();
    }
}

/*****************************************************************
 *      Log data in transparent format
 *****************************************************************/
PUBLIC void _log_bf(int priority, log_opt_t opt, const char *bf, int len)
{
    if(!__initialized__) {
        return;
    }
    if(len <= 0) {
        return;
    }
#ifdef USE_MUTEX
    uv_mutex_lock(&mutex_log);
#endif
    log_handler_t *lh = dl_first(&dl_clients);
    while(lh) {
        if(must_ignore(lh, priority)) {
            /*
             *  Next
             */
            lh = dl_next(lh);
            continue;
        }
        if(lh->hr->write_fn) {
            int ret = (lh->hr->write_fn)(lh->h, priority, bf, len);
            if(ret < 0) { // Handler owns the message
                break;
            }
        }
        if(lh->hr->fwrite_fn) {
            if(opt & (LOG_OPT_TRACE_STACK|LOG_OPT_EXIT_NEGATIVE|LOG_OPT_ABORT)) {
                show_backtrace(lh->hr->fwrite_fn, lh->h);
            }
        }

        /*
         *  Next
         */
        lh = dl_next(lh);
    }
#ifdef USE_MUTEX
    uv_mutex_unlock(&mutex_log);
#endif
    if(opt & LOG_OPT_EXIT_NEGATIVE) {
        exit(-1);
    }
    if(opt & LOG_OPT_EXIT_ZERO) {
        exit(0);
    }
    if(opt & LOG_OPT_ABORT) {
        abort();
    }
}

/*****************************************************************
 *  Add key/values from va_list argument
 *****************************************************************/
PRIVATE void json_vappend(hgen_t hgen, va_list ap)
{
    char *key;
    char *fmt;
    size_t i;
    char value[4*1024];

    while ((key = (char *)va_arg (ap, char *)) != NULL) {
        fmt = (char *)va_arg (ap, char *);
        for (i = 0; i < strlen (fmt); i++) {
            int eof = 0;

            if (fmt[i] != '%')
                continue;
            i++;
            while (eof != 1) {
                switch (fmt[i]) {
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X':
                        if (fmt[i - 1] == 'l') {
                            if (i - 2 > 0 && fmt[i - 2] == 'l') {
                                long long int v;
                                v = va_arg(ap, long long int);
                                json_add_integer(hgen, key, v);

                            } else {
                                long int v;
                                v = va_arg(ap, long int);
                                json_add_integer(hgen, key, v);
                            }
                        } else {
                            int v;
                            v = va_arg(ap, int);
                            json_add_integer(hgen, key, v);
                        }
                        eof = 1;
                        break;
                    case 'e':
                    case 'E':
                    case 'f':
                    case 'F':
                    case 'g':
                    case 'G':
                    case 'a':
                    case 'A':
                        if (fmt[i - 1] == 'L') {
                            long double v;
                            v = va_arg (ap, long double);
                            json_add_double(hgen, key, v);
                        } else {
                            double v;
                            v = va_arg (ap, double);
                            json_add_double(hgen, key, v);
                        }
                        eof = 1;
                        break;
                    case 'c':
                        if (fmt [i - 1] == 'l') {
#ifdef _WIN32
                            (void)va_arg (ap, int);
#else
                            (void)va_arg (ap, wint_t);
#endif
                        } else {
                            (void)va_arg (ap, int);
                        }
                        eof = 1;
                        break;
                    case 's':
                        if (fmt [i - 1] == 'l') {
                            wchar_t *p;
                            size_t len;

                            p = va_arg (ap, wchar_t *);
                            if(p && (len = snprintf(value, sizeof(value), "%ls", p))>=0) {
                                if(strcmp(key, "msg")==0) {
                                    snprintf(last_message, sizeof(last_message), "%s", value);
                                }
                                json_add_string(hgen, key, value);
                            } else {
                                json_add_null(hgen, key);
                            }

                        } else {
                            char *p;
                            int len;

                            p = va_arg (ap, char *);
                            if(p && (len = snprintf(value, sizeof(value), "%s", p))>=0) {
                                if(strcmp(key, "msg")==0) {
                                    snprintf(last_message, sizeof(last_message), "%s", value);
                                }
                                json_add_string(hgen, key, value);
                            } else {
                                json_add_null(hgen, key);
                            }
                        }
                        eof = 1;
                        break;
                    case 'p':
                        {
                            void *p;
                            int len;

                            p = va_arg (ap, void *);
                            eof = 1;
                            if(p && (len = snprintf(value, sizeof(value), "%p", (char *)p))>0) {
                                json_add_string(hgen, key, value);
                            } else {
                                json_add_null(hgen, key);
                            }
                        }
                        break;
                    case 'j':
                        {
                            json_t *jn;

                            jn = va_arg (ap, void *);
                            eof = 1;

                            if(jn) {
                                size_t flags = JSON_ENCODE_ANY |
                                    JSON_INDENT(0) |
                                    JSON_REAL_PRECISION(get_real_precision());
                                char *bf = json_dumps(jn, flags);
                                if(bf) {
                                    helper_doublequote2quote(bf);
                                    json_add_string(hgen, key, bf);
                                    jsonp_free(bf) ;
                                } else {
                                    json_add_null(hgen, key);
                                }
                            } else {
                                json_add_null(hgen, key);
                            }
                        }
                        break;
                    case '%':
                        eof = 1;
                        break;
                    default:
                        i++;
                }
            }
        }
    }
}

/*****************************************************************
 *  Add key/values from ... argument
 *****************************************************************/
PRIVATE void json_append(hgen_t hgen, ...)
{
    va_list ap;

    va_start(ap, hgen);
    json_vappend(hgen, ap);
    va_end(ap);
}

/*****************************************************************
 *      Discover extra data
 *****************************************************************/
PRIVATE void discover(hgen_t hgen)
{
    json_append(hgen,
        "process",      "%s", get_process_name(),
        "hostname",     "%s", get_host_name(),
        "pid",          "%d", get_pid(),
        NULL
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void show_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h)
{
#ifndef NOT_INCLUDE_LIBUNWIND
    static int inside = 0;

    if(inside) {
        return;
    }
    inside = 1;
    char name[256+10];
    unw_cursor_t cursor; unw_context_t uc;
    unw_word_t ip, sp, offp;

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);

    fwrite_fn(h, LOG_DEBUG, "===============> begin stack trace <==================");
    fwrite_fn(h, LOG_DEBUG, "binary: %s", __executable__);

    while (unw_step(&cursor) > 0) {
        name[0] = '\0';
        unw_get_proc_name(&cursor, name, 256, &offp);
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        strcat(name, "()");

        fwrite_fn(h, LOG_DEBUG, "%-32s ip = 0x%llx, sp = 0x%llx, off = 0x%llx",
            name,
            (long long) ip,
            (long long) sp,
            (long long) offp
        );
    }
    fwrite_fn(h, LOG_DEBUG, "===============> end stack trace <==================");
    inside = 0;
#endif /* NOT_INCLUDE_LIBUNWIND */
}

/***************************************************************************
 *  Set real precision
 *  Return the previous precision
 ***************************************************************************/
PRIVATE int __real_precision__ = 12;
PUBLIC int set_real_precision(int precision)
{
    int prev = __real_precision__;
    __real_precision__ = precision;
    return prev;
}
PUBLIC int get_real_precision(void)
{
    return __real_precision__;
}

