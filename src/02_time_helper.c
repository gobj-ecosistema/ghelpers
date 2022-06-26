/****************************************************************************
 *              TIME_HELPER.C
 *              Copyright (c) 2013 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include "02_time_helper.h"

/*****************************************************************
 *          Constants
 *****************************************************************/

/*****************************************************************
 *  timestamp with usec resolution
 *  `bf` must be 90 bytes minimum
 *****************************************************************/
PUBLIC char *current_timestamp(char *bf, int bfsize)
{
    // HACK el uso de variables locales intermedias facilita el debugging, pero lo hace mucho mas LENTO!!!
    struct timespec ts;
    struct tm *tm;
    char stamp[64], zone[16];
    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&ts.tv_sec);

    strftime(stamp, sizeof (stamp), "%Y-%m-%dT%H:%M:%S", tm);
    strftime(zone, sizeof (zone), "%z", tm);
    snprintf(bf, bfsize, "%s.%09lu%s", stamp, ts.tv_nsec, zone);
    return bf;
}

/*****************************************************************
 *  Get timestamp from tm struct
 *****************************************************************/
PUBLIC char *tm2timestamp(char *bf, int bfsize, struct tm *tm)
{
    strftime(bf, bfsize, "%Y-%m-%dT%H:%M:%S.0%z", tm);
    return bf;
}

/*****************************************************************
 *  Convert GMT time `t` (seconds from Epoch) to Time Zone `tz`
 *  filling `ltm` and `offset`.
 *  Return t plus the offset of search time zone
 *  (use gmtime with the returned value)
 *  tz values in
 *      https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
 *  Example: time_t offset = gmtime2timezone(0, ":America/Panama", 0, 0);
 *  WARNING use ':' in TZ name
 *****************************************************************/
PUBLIC time_t gmtime2timezone(time_t t, const char *tz, struct tm *ltm, time_t *offset)
{
    /*
     *  Save current TZ of env
     */
    char *cur_tz = getenv("TZ");
    if (cur_tz) {
        cur_tz = strdup(cur_tz);
    }

    /*
     *  Set searched TZ in env
     */
    setenv("TZ", tz, 1);
    tzset();

    /*
     *  Get localtime of t with searched TZ
     */
    if(ltm) {
        localtime_r(&t, ltm);
    }

    /*
     *  Return offset of the searched TZ
     */
    time_t t_epoch = time(NULL);
    struct tm mt_epoch = {0};

    localtime_r(&t_epoch, &mt_epoch);
    if(offset) {
        *offset = mt_epoch.tm_gmtoff;
    }

    /*
     *  Restore current TZ of env
     */
    if (cur_tz) {
        setenv("TZ", cur_tz, 1);
        free(cur_tz);
    } else {
        unsetenv("TZ");
    }
    tzset();

    /*
     *  Return t plus the offset of searched TZ
     */
    t += mt_epoch.tm_gmtoff;
    return t;
}

/***********************************************************************
 *   Get a string with some now! date formatted
 *   DEPRECATED use strftime()
 ***********************************************************************/
PUBLIC char *formatdate(time_t t, char *bf, int bfsize, const char *format)
{
    char sfechahora[32];

    struct tm *tm;
    tm = localtime(&t);

    /* Pon en formato DD/MM/CCYY-W-ZZZ */
    snprintf(sfechahora, sizeof(sfechahora), "%02d/%02d/%4d-%d-%03d",
        tm->tm_mday,            // 01-31
        tm->tm_mon+1,           // 01-12
        tm->tm_year + 1900,
        tm->tm_wday+1,          // 1-7
        tm->tm_yday+1           // 001-365
    );
    if(empty_string(format)) {
        format = "DD/MM/CCYY-W-ZZZ";
    }

    translate_string(bf, bfsize, sfechahora, format, "DD/MM/CCYY-W-ZZZ");

    return bf;
}

/***********************************************************************
 *   Get a string with some now! date or time formatted
 ***********************************************************************/
PUBLIC char *formatnowdate(char *bf, int bfsize, const char *format)
{
    time_t t;

    time(&t);

    return formatdate(t, bf, bfsize, format);
}

/***********************************************************************
 *   Print nicely a diff time
 ***********************************************************************/
PUBLIC int nice_difftime(char *bf, int bfsize, uint32_t diff_time)
{
    uint32_t seconds = diff_time;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    uint32_t weeks = days / 7;
    uint32_t months = weeks / 52;
    uint32_t years = months / 12;

    if(years) {
        snprintf(bf, bfsize, "%dY%dM%dD", years, months, days);
    } else if(months) {
        snprintf(bf, bfsize, "%dM%dD%dh", months, days, hours);
    } else if(days) {
        snprintf(bf, bfsize, "%dD%dh%dm", days, hours, minutes);
    } else if(hours) {
        snprintf(bf, bfsize, "%dh%dm%ds", hours, minutes, seconds);
    } else if(minutes) {
        snprintf(bf, bfsize, "%dm%ds", minutes, seconds);
    } else {
        snprintf(bf, bfsize, "%ds", seconds);
    }


    return 0;
}

/****************************************************************************
 *   Arranca un timer de 'seconds' segundos.
 *   El valor retornado es el que hay que usar en la funcion test_timer()
 *   para ver si el timer ha cumplido.
 ****************************************************************************/
PUBLIC time_t start_sectimer(time_t seconds)
{
    time_t timer;

    time(&timer);
    timer += seconds;
    return timer;
}

/****************************************************************************
 *   Retorna TRUE si ha cumplido el timer 'value', FALSE si no.
 ****************************************************************************/
PUBLIC BOOL test_sectimer(time_t value)
{
    time_t timer_actual;

    if(value <= 0) {
        return FALSE;
    }
    time(&timer_actual);
    return (timer_actual>=value)? TRUE:FALSE;
}

/****************************************************************************
 *   Arranca un timer de 'miliseconds' mili-segundos.
 *   El valor retornado es el que hay que usar en la funcion test_msectimer()
 *   para ver si el timer ha cumplido.
 ****************************************************************************/
PUBLIC uint64_t start_msectimer(uint64_t miliseconds)
{
    uint64_t ms = time_in_miliseconds();
    ms += miliseconds;
    return ms;
}

/****************************************************************************
 *   Retorna TRUE si ha cumplido el timer 'value', FALSE si no.
 ****************************************************************************/
PUBLIC BOOL test_msectimer(uint64_t value)
{
    if(value == 0) {
        return FALSE;
    }

    uint64_t ms = time_in_miliseconds();

    return (ms>value)? TRUE:FALSE;
}

/****************************************************************************
 *  Current time in milisecons
 ****************************************************************************/
PUBLIC uint64_t time_in_miliseconds(void)
{
    struct timespec spec;

    //clock_gettime(CLOCK_MONOTONIC, &spec); //Este no da el time from Epoch
    clock_gettime(CLOCK_REALTIME, &spec);

    // Convert to milliseconds
    return (uint64_t)spec.tv_sec*1000 + spec.tv_nsec/1000000;
}

/***************************************************************************
 *  Return current time in seconds (standart time(&t))
 ***************************************************************************/
PUBLIC uint64_t time_in_seconds(void)
{
    time_t __t__;
    time(&__t__);
    return (uint64_t) __t__;
}

/***************************************************************************
 *  Return in gmt time range in hours of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_hours_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);
    } else {
        gmtime_r(&t, &tm);
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mon = tm.tm_mon;
    tm_x.tm_mday = tm.tm_mday;
    tm_x.tm_hour = tm.tm_hour;

    time_range.start = mktime(&tm_x);
    tm_x.tm_hour += range;
    time_range.end = mktime(&tm_x);

    return time_range;
}

/***************************************************************************
 *  Return in gmt time range in days of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_days_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);
    } else {
        gmtime_r(&t, &tm);
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mon = tm.tm_mon;
    tm_x.tm_mday = tm.tm_mday;

    time_range.start = mktime(&tm_x);
    tm_x.tm_mday += range;
    time_range.end = mktime(&tm_x);

    return time_range;
}

/***************************************************************************
 *  Return in gmt time range in weeks of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_weeks_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);

    } else {
        gmtime_r(&t, &tm);
    }

    int day = tm.tm_mday;
    int wday= tm.tm_wday;
    if(wday == 1) {
        // Estamos en lunes
        day = tm.tm_mday;
    } else if(wday == 0) {
        // Domingo
        day += -6;
    } else { //if(wday > 1) {
        // Dentro de la semana
        day += (1 - wday);
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mon = tm.tm_mon;
    tm_x.tm_mday = day;

    time_range.start = mktime(&tm_x);
    tm_x.tm_mday += range*7;
    time_range.end = mktime(&tm_x);

    return time_range;
}

/***************************************************************************
 *  Return in gmt time range in month of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_months_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);
    } else {
        gmtime_r(&t, &tm);
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mon = tm.tm_mon;
    tm_x.tm_mday = 1;

    time_range.start = mktime(&tm_x);
    tm_x.tm_mon += range;
    time_range.end = mktime(&tm_x);

    return time_range;
}

/***************************************************************************
 *  Return in gmt time range in year of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_years_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);
    } else {
        gmtime_r(&t, &tm);
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mday = 1;

    time_range.start = mktime(&tm_x);
    tm_x.tm_year += range;
    time_range.end = mktime(&tm_x);

    return time_range;
}
