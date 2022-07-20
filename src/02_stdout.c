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
#ifndef WIN32
#include <unistd.h>
#endif
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

    if(priority <= LOG_ERR) {
        fwrite(On_Red, strlen(On_Red), 1, stdout);
        fwrite(BWhite, strlen(BWhite), 1, stdout);
        fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
        fwrite(Color_Off, strlen(Color_Off), 1, stdout);

    } else if(priority <= LOG_WARNING) {
        fwrite(On_Black, strlen(On_Black), 1, stdout);
        fwrite(BYellow, strlen(BYellow), 1, stdout);
        fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
        fwrite(Color_Off, strlen(Color_Off), 1, stdout);
    } else {
        fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
    }

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
    va_list aq;
    int length;
    char *buf;

    va_start(ap, fmt);
    va_copy(aq, ap);

    length = vsnprintf(NULL, 0, fmt, ap);
    if(length>0) {
        buf = malloc(length + 1);
        if(buf) {
            vsnprintf(buf, length + 1, fmt, aq);
            stdout_write(v, priority, buf, strlen(buf));
            free(buf);
        }
    }

    va_end(ap);
    va_end(aq);

    return 0;
}
