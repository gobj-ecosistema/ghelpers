/****************************************************************************
 *              PRINT_ERROR.C
 *              Copyright (c) 2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __linux__
#include <syslog.h>
#include <unistd.h>
#endif

#ifndef NOT_INCLUDE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#include "01_print_error.h"

static char last_error[BUFSIZ];

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void show_backtrace(void)
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

    fprintf(stderr, "===============> begin stack trace <==================\n");

    while (unw_step(&cursor) > 0) {
        name[0] = '\0';
        unw_get_proc_name(&cursor, name, 256, &offp);
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        strcat(name, "()");

        fprintf(stderr, "%-32s ip = 0x%llx, sp = 0x%llx, off = 0x%llx\n",
            name,
            (long long) ip,
            (long long) sp,
            (long long) offp
        );
    }
    fprintf(stderr, "===============> end stack trace <==================\n\n");
    inside = 0;
#endif /* NOT_INCLUDE_LIBUNWIND */
}

/***************************************************************************
 *  print error in syslog and console.
 ***************************************************************************/
PUBLIC void print_error(pe_flag_t quit, const char *prefix, const char *fmt, ...)
{
    char temp[16*1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(temp, sizeof(temp), fmt, ap);
    va_end(ap);

    strncpy(last_error, temp, sizeof(last_error)-1);

    if(quit == PEF_SILENCE) {
        return;
    }

    fprintf(stderr, "%s: %s\r\n\r\n", prefix, temp);
#ifdef __linux__
    syslog(LOG_ERR, "%s: %s", prefix, temp);
#endif
    switch(quit) {
        case PEF_ABORT:
            show_backtrace();
            abort();
        case PEF_EXIT:
            show_backtrace();
            exit(-1);
        case PEF_SILENCE:   // to avoid warning
        case PEF_CONTINUE:
            break;
    }
}

/***************************************************************************
 *  print error in syslog and console.
 ***************************************************************************/
PUBLIC void print_info(const char *prefix, const char *fmt, ...)
{
    char temp[BUFSIZ];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(temp, sizeof(temp), fmt, ap);
    va_end(ap);

    fprintf(stderr, "%s: %s\r\n\r\n", prefix, temp);
#ifdef __linux__
    syslog(LOG_INFO, "%s: %s", prefix, temp);
#endif
}

/***************************************************************************
 *  Return a string description of the last error
 ***************************************************************************/
PUBLIC const char *get_last_error(void)
{
    return last_error;
}
