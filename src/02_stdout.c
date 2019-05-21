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
    static char buffer[1*1024*1024];
    if(!bf) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "stdout_write(): bf NULL"
        );
        return -1;
    }
    if(len<=0) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "stdout_write(): len <= 0"
        );
        return -1;
    }
    if(priority < 0 || priority > LOG_MONITOR) {
        priority = LOG_DEBUG;
    }

//     if(priority == LOG_DEBUG) {
//         snprintf(
//             buffer,
//             sizeof(buffer),
//             "%.*s\n",
//             (int)len,
//             bf
//         );
//     } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "%s: %.*s\n",
            priority_names[priority],
            (int)len,
            bf
        );
//     }
    fwrite(buffer, strlen(buffer), 1, stdout);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int stdout_fwrite(void *v, int priority, const char *format, ...)
{
    va_list ap;
    static char temp[8*1024];

    va_start(ap, format);
    vsnprintf(
        temp,
        sizeof(temp),
        format,
        ap
    );
    va_end(ap);

    return stdout_write(v, priority, temp, strlen(temp));
}
