/****************************************************************************
 *              TIME_HELPER.H
 *              Copyright (c) 2013 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <time.h>
#include <stdint.h>
#include "00_gtypes.h"

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
 *     Structures
 *****************************************************************/
typedef struct {
    time_t start;
    time_t end;
} time_range_t;

/*****************************************************************
 *     Prototypes
 *****************************************************************/

// `bf` must be 90 bytes minimum
PUBLIC char *current_timestamp(char *bf, int bfsize);
PUBLIC char *tm2timestamp(char *bf, int bfsize, struct tm *tm);

// tz values in https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
// Example: time_t offset = gmtime2timezone(0, ":America/Panama", 0, 0); WARNING use ':' in TZ name

PUBLIC time_t gmtime2timezone(time_t t, const char *tz, struct tm *ltm, time_t *offset);

/*
 *  Default format mask if ``format`` is empty: "DD/MM/CCYY-W-ZZZ"
 */
PUBLIC char *formatdate(time_t t, char *bf, int bfsize, const char *format); // DEPRECATED use strftime()
PUBLIC char *formatnowdate(char *bf, int bfsize, const char *format);

PUBLIC int nice_difftime(char *bf, int bfsize, uint32_t diff_time);


PUBLIC time_t start_sectimer(time_t seconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_sectimer(time_t value);      /* Return TRUE if timer has finish */

PUBLIC uint64_t start_msectimer(uint64_t miliseconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_msectimer(uint64_t value);           /* Return TRUE if timer has finish */

PUBLIC uint64_t time_in_miliseconds(void);   // Return current time in miliseconds
PUBLIC uint64_t time_in_seconds(void);       // Return current time in seconds (standart time(&t))

/**rst**
    Return in gmt time range in hours of time t, TZ optional
**rst**/
PUBLIC time_range_t get_hours_range(time_t t, int range, const char *TZ);

/**rst**
    Return in gmt time range in days of time t, TZ optional
**rst**/
PUBLIC time_range_t get_days_range(time_t t, int range, const char *TZ);

/**rst**
    Return in gmt time range in weeks of time t, TZ optional
**rst**/
PUBLIC time_range_t get_weeks_range(time_t t, int range, const char *TZ);

/**rst**
    Return in gmt time range in month of time t, TZ optional
**rst**/
PUBLIC time_range_t get_months_range(time_t t, int range, const char *TZ);

/**rst**
    Return in gmt time range in year of time t, TZ optional
**rst**/
PUBLIC time_range_t get_years_range(time_t t, int range, const char *TZ);

#ifdef __cplusplus
}
#endif
