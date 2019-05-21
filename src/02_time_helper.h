/****************************************************************************
 *              TIME_HELPER.H
 *              Copyright (c) 2013 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/

#ifndef _TIME_HELPER_H
#define _TIME_HELPER_H 1

#include <time.h>
#include <stdint.h>

/*
 *  Dependencies
 */
#include "01_gstrings.h"
#include "01_print_error.h"

#ifdef __cplusplus
extern "C"{
#endif

/********************************************************************
 *        Constants
 ********************************************************************/
#define SECONDS_OF_ONE_DAY (60L*60L*24L)
#define SECONDS_OF_ONE_WEEK (60L*60L*24L*7L)
#define SECONDS_OF_ONE_MONTH (60L*60L*24L*31L)


/*****************************************************************
 *     Prototypes
 *****************************************************************/

// `bf` must be 90 bytes minimum
PUBLIC char *current_timestamp(char *bf, int bfsize);
PUBLIC char *tm2timestamp(char *bf, int bfsize, struct tm *tm);

// tz values in https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
PUBLIC time_t gmtime2timezone(time_t t, const char *tz, struct tm *ltm, time_t *offset);

/*
 *  Default format mask if ``format`` is empty: "DD/MM/CCYY-W-ZZZ"
 */
PUBLIC char *formatdate(time_t t, char *bf, int bfsize, const char *format); // DEPRECATED use strftime()
PUBLIC char *formatnowdate(char *bf, int bfsize, const char *format);

PUBLIC int nice_difftime(char *bf, int bfsize, uint32_t diff_time);


PUBLIC time_t start_sectimer(time_t seconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_sectimer(time_t value);      /* Return TRUE if timer has finish */

PUBLIC int64_t start_msectimer(int64_t miliseconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_msectimer(int64_t value);           /* Return TRUE if timer has finish */

PUBLIC int64_t time_in_miliseconds(void);   // Return current time in miliseconds
PUBLIC int64_t time_in_seconds(void);       // Return current time in seconds (standart time(&t))

#ifdef __cplusplus
}
#endif

#endif /* _TIME_HELPER_H */
