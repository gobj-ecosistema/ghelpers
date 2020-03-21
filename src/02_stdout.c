/***********************************************************************
 *          STDOUT.C
 *
 *          GLogger handler for stdout
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "02_stdout.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const char *priority_names[]={
    "EMERG",
    "ALERT",
    "CRITICAL",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
    "STATS",
    "MONITOR",
    "AUDIT",
    0
};

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int stdout_write(void* v, int priority, const char* bf, size_t len)
{
    if(!bf) {
        // silence
        return -1;
    }
    if(len<=0) {
        // silence
        return -1;
    }
    if(priority < 0 || priority > LOG_MONITOR) {
        priority = LOG_DEBUG;
    }

    fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
    fwrite(": ", strlen(": "), 1, stdout);
    fwrite(bf, len, 1, stdout);
    fwrite("\n", strlen("\n"), 1, stdout);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int stdout_fwrite(void *v, int priority, const char *fmt, ...)
{
    va_list ap;
    int length;
    char *buf;

    va_start(ap, fmt);

    length = vsnprintf(NULL, 0, fmt, ap);
    if(length>0) {
        buf = malloc(length + 1);
        if(buf) {
            vsnprintf(buf, length + 1, fmt, ap);
            stdout_write(v, priority, buf, strlen(buf));
            free(buf);
        }
    }

    va_end(ap);

    return 0;
}
