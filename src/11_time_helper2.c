/****************************************************************************
 *              TIME_HELPER2.H
 *              Code from git/date.c
 *
 *
 check_approxidate 'noon today' '2009-08-30 12:00:00'
 check_approxidate 'noon yesterday' '2009-08-29 12:00:00'
 check_approxidate 'January 5th noon pm' '2009-01-05 12:00:00'
+check_approxidate '10am noon' '2009-08-29 12:00:00'

 check_approxidate 'last tuesday' '2009-08-25 19:20:00'
 check_approxidate 'July 5th' '2009-07-05 19:20:00'

 *              Copyright (C) Linus Torvalds, 2005
 *              Copyright (c) Niyamaka, 2018.
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <inttypes.h>
#include <time.h>
#ifdef WIN32
    #include <io.h>
#else
    #include <unistd.h>
    #include <sys/time.h>
#endif
#include "11_time_helper2.h"

#define _

/*
 * This is like mktime, but without normalization of tm_wday and tm_yday.
 */
time_t tm_to_time_t(const struct tm *tm)
{
    static const int mdays[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int year = tm->tm_year - 70;
    int month = tm->tm_mon;
    int day = tm->tm_mday;

    if (year < 0 || year > 129) /* algo only works for 1970-2099 */
        return -1;
    if (month < 0 || month > 11) /* array bounds */
        return -1;
    if (month < 2 || (year + 2) % 4)
        day--;
    if (tm->tm_hour < 0 || tm->tm_min < 0 || tm->tm_sec < 0)
        return -1;
    return (year * 365 + (year + 1) / 4 + mdays[month] + day) * 24*60*60UL +
        tm->tm_hour * 60*60 + tm->tm_min * 60 + tm->tm_sec;
}

static const char *month_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const char *weekday_names[] = {
    "Sundays", "Mondays", "Tuesdays", "Wednesdays", "Thursdays", "Fridays", "Saturdays"
};

static time_t gm_time_t(timestamp_t time, int tz)
{
    int minutes;

    minutes = tz < 0 ? -tz : tz;
    minutes = (minutes / 100)*60 + (minutes % 100);
    minutes = tz < 0 ? -minutes : minutes;

    if (minutes > 0) {
        if (unsigned_add_overflows(time, minutes * 60))
            trace_msg("Timestamp+tz too large: %"PRItime" +%04d",
                time, tz);
    } else if (time < -minutes * 60)
        trace_msg("Timestamp before Unix epoch: %"PRItime" %04d", time, tz);
    time += minutes * 60;
    if (date_overflows(time))
        trace_msg("Timestamp too large for this system: %"PRItime, time);
    return (time_t)time;
}

/*
 * The "tz" thing is passed in as this strange "decimal parse of tz"
 * thing, which means that tz -0100 is passed in as the integer -100,
 * even though it means "sixty minutes off"
 */
static struct tm *time_to_tm(timestamp_t time, int tz, struct tm *tm)
{
    time_t t = gm_time_t(time, tz);
#ifdef WIN32
    gmtime_s(tm, &t);
    return tm;
#else
    return gmtime_r(&t, tm);
#endif
}

static struct tm *time_to_tm_local(timestamp_t time, struct tm *tm)
{
    time_t t = time;
#ifdef WIN32
    localtime_s(tm, &t);
    return tm;
#else
    return localtime_r(&t, tm);
#endif
}

/*
 * Fill in the localtime 'struct tm' for the supplied time,
 * and return the local tz.
 */
static int local_time_tzoffset(time_t t, struct tm *tm)
{
    time_t t_local;
    int offset, eastwest;

#ifdef WIN32
    localtime_s(tm, &t);
#else
    localtime_r(&t, tm);
#endif
    t_local = tm_to_time_t(tm);
    if (t_local == -1)
        return 0; /* error; just use +0000 */
    if (t_local < t) {
        eastwest = -1;
        offset = t - t_local;
    } else {
        eastwest = 1;
        offset = t_local - t;
    }
    offset /= 60; /* in minutes */
    offset = (offset % 60) + ((offset / 60) * 100);
    return offset * eastwest;
}

/*
 * What value of "tz" was in effect back then at "time" in the
 * local timezone?
 */
static int local_tzoffset(timestamp_t time)
{
    struct tm tm;

    if (date_overflows(time))
        trace_msg("Timestamp too large for this system: %"PRItime, time);

    return local_time_tzoffset((time_t)time, &tm);
}

/***********************************************************************
 *   Get a string with some now! date or time formatted
 ***********************************************************************/
void show_date_relative(
    timestamp_t tim,
    char *timebuf,
    int timebufsize)
{
    time_t tv_sec;
    timestamp_t diff;

    time(&tv_sec);
    if (tv_sec < tim) {
        snprintf(timebuf, timebufsize, _("in the future"));
        return;
    }
    diff = tv_sec - tim;
    if (diff < 90) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" seconds ago"), diff);
        return;
    }
    /* Turn it into minutes */
    diff = (diff + 30) / 60;
    if (diff < 90) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" minutes ago"), diff);
        return;
    }
    /* Turn it into hours */
    diff = (diff + 30) / 60;
    if (diff < 36) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" hours ago"), diff);
        return;
    }
    /* We deal with number of days from here on */
    diff = (diff + 12) / 24;
    if (diff < 14) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" days ago"), diff);
        return;
    }
    /* Say weeks for the past 10 weeks or so */
    if (diff < 70) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" weeks ago"),
             (diff + 3) / 7);
        return;
    }
    /* Say months for the past 12 months or so */
    if (diff < 365) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" months ago"),
             (diff + 15) / 30);
        return;
    }
    /* Give years and months for 5 years or so */
    if (diff < 1825) {
        timestamp_t totalmonths = (diff * 12 * 2 + 365) / (365 * 2);
        timestamp_t years = totalmonths / 12;
        timestamp_t months = totalmonths % 12;
        if (months) {
            char sb[120];
            snprintf(sb, sizeof(sb), _("%"PRItime" years"), years);
            snprintf(timebuf, timebufsize,
                 /* TRANSLATORS: "%s" is "<n> years" */
                 _("%s, %"PRItime" months ago"),
                 sb, months);
        } else
            snprintf(timebuf, timebufsize,
                 _("%"PRItime" years ago"), years);
        return;
    }
    /* Otherwise, just years. Centuries is probably overkill. */
    snprintf(timebuf, timebufsize,
         _("%"PRItime" years ago"),
         (diff + 183) / 365);
}

struct date_mode *date_mode_from_type(enum date_mode_type type)
{
    static struct date_mode mode;
    if (type == DATE_STRFTIME)
        trace_msg("cannot create anonymous strftime date_mode struct");
    mode.type = type;
    return &mode;
}

const char *show_date(timestamp_t tim, int tz, const struct date_mode *mode)
{
    struct tm *tm;
    static char timebuf[1024];
    struct tm tmbuf = { 0 };

    if (mode->type == DATE_UNIX) {
        snprintf(timebuf, sizeof(timebuf), "%"PRItime, tim);
        return timebuf;
    }

    if (mode->local)
        tz = local_tzoffset(tim);

    if (mode->type == DATE_RAW) {
        snprintf(timebuf, sizeof(timebuf), "%"PRItime" %+05d", tim, tz);
        return timebuf;
    }

    if (mode->type == DATE_RELATIVE) {
        show_date_relative(tim, timebuf, sizeof(timebuf));
        return timebuf;
    }

    if (mode->local)
        tm = time_to_tm_local(tim, &tmbuf);
    else
        tm = time_to_tm(tim, tz, &tmbuf);
    if (!tm) {
        tm = time_to_tm(0, 0, &tmbuf);
        tz = 0;
    }

    if (mode->type == DATE_SHORT)
        snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02d", tm->tm_year + 1900,
            tm->tm_mon + 1, tm->tm_mday);
    else if (mode->type == DATE_ISO8601)
        snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02d %02d:%02d:%02d %+05d",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            tz);
    else if (mode->type == DATE_ISO8601_STRICT) {
        char sign = (tz >= 0) ? '+' : '-';
        tz = abs(tz);
        snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            sign, tz / 100, tz % 100);
    } else if (mode->type == DATE_RFC2822)
        snprintf(timebuf, sizeof(timebuf), "%.3s, %d %.3s %d %02d:%02d:%02d %+05d",
            weekday_names[tm->tm_wday], tm->tm_mday,
            month_names[tm->tm_mon], tm->tm_year + 1900,
            tm->tm_hour, tm->tm_min, tm->tm_sec, tz);
    else if (mode->type == DATE_STRFTIME)
        snprintf(timebuf, sizeof(timebuf), mode->strftime_fmt, tm, tz,
            !mode->local);
    else
        snprintf(timebuf, sizeof(timebuf), "%.3s %.3s %d %02d:%02d:%02d %d%c%+05d",
            weekday_names[tm->tm_wday],
            month_names[tm->tm_mon],
            tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            tm->tm_year + 1900,
            mode->local ? 0 : ' ',
            tz);
    return timebuf;
}

/*
 * Check these. And note how it doesn't do the summer-time conversion.
 *
 * In my world, it's always summer, and things are probably a bit off
 * in other ways too.
 */
static const struct {
    const char *name;
    int offset;
    int dst;
} timezone_names[] = {
    { "IDLW", -12, 0, },    /* International Date Line West */
    { "NT",   -11, 0, },    /* Nome */
    { "CAT",  -10, 0, },    /* Central Alaska */
    { "HST",  -10, 0, },    /* Hawaii Standard */
    { "HDT",  -10, 1, },    /* Hawaii Daylight */
    { "YST",   -9, 0, },    /* Yukon Standard */
    { "YDT",   -9, 1, },    /* Yukon Daylight */
    { "PST",   -8, 0, },    /* Pacific Standard */
    { "PDT",   -8, 1, },    /* Pacific Daylight */
    { "MST",   -7, 0, },    /* Mountain Standard */
    { "MDT",   -7, 1, },    /* Mountain Daylight */
    { "CST",   -6, 0, },    /* Central Standard */
    { "CDT",   -6, 1, },    /* Central Daylight */
    { "EST",   -5, 0, },    /* Eastern Standard */
    { "EDT",   -5, 1, },    /* Eastern Daylight */
    { "AST",   -3, 0, },    /* Atlantic Standard */
    { "ADT",   -3, 1, },    /* Atlantic Daylight */
    { "WAT",   -1, 0, },    /* West Africa */

    { "GMT",    0, 0, },    /* Greenwich Mean */
    { "UTC",    0, 0, },    /* Universal (Coordinated) */
    { "Z",      0, 0, },    /* Zulu, alias for UTC */

    { "WET",    0, 0, },    /* Western European */
    { "BST",    0, 1, },    /* British Summer */
    { "CET",   +1, 0, },    /* Central European */
    { "MET",   +1, 0, },    /* Middle European */
    { "MEWT",  +1, 0, },    /* Middle European Winter */
    { "MEST",  +1, 1, },    /* Middle European Summer */
    { "CEST",  +1, 1, },    /* Central European Summer */
    { "MESZ",  +1, 1, },    /* Middle European Summer */
    { "FWT",   +1, 0, },    /* French Winter */
    { "FST",   +1, 1, },    /* French Summer */
    { "EET",   +2, 0, },    /* Eastern Europe, USSR Zone 1 */
    { "EEST",  +2, 1, },    /* Eastern European Daylight */
    { "WAST",  +7, 0, },    /* West Australian Standard */
    { "WADT",  +7, 1, },    /* West Australian Daylight */
    { "CCT",   +8, 0, },    /* China Coast, USSR Zone 7 */
    { "JST",   +9, 0, },    /* Japan Standard, USSR Zone 8 */
    { "EAST", +10, 0, },    /* Eastern Australian Standard */
    { "EADT", +10, 1, },    /* Eastern Australian Daylight */
    { "GST",  +10, 0, },    /* Guam Standard, USSR Zone 9 */
    { "NZT",  +12, 0, },    /* New Zealand */
    { "NZST", +12, 0, },    /* New Zealand Standard */
    { "NZDT", +12, 1, },    /* New Zealand Daylight */
    { "IDLE", +12, 0, },    /* International Date Line East */
};

static int match_string(const char *date, const char *str)
{
    int i;

    for (i = 0; *date; date++, str++, i++) {
        if (*date == *str)
            continue;
        if (toupper(*date) == toupper(*str))
            continue;
        if (!isalnum(*((unsigned char *)date)))
            break;
        return 0;
    }
    return i;
}

static int skip_alpha(const char *date)
{
    int i = 0;
    do {
        i++;
    } while (isalpha(((unsigned char *)date)[i]));
    return i;
}

/*
* Parse month, weekday, or timezone name
*/
static int match_alpha(const char *date, struct tm *tm, int *offset)
{
    int i;

    for (i = 0; i < 12; i++) {
        int match = match_string(date, month_names[i]);
        if (match >= 3) {
            tm->tm_mon = i;
            return match;
        }
    }

    for (i = 0; i < 7; i++) {
        int match = match_string(date, weekday_names[i]);
        if (match >= 3) {
            tm->tm_wday = i;
            return match;
        }
    }

    for (i = 0; i < ARRAY_SIZE(timezone_names); i++) {
        int match = match_string(date, timezone_names[i].name);
        if (match >= 3 || match == strlen(timezone_names[i].name)) {
            int off = timezone_names[i].offset;

            /* This is bogus, but we like summer */
            off += timezone_names[i].dst;

            /* Only use the tz name offset if we don't have anything better */
            if (*offset == -1)
                *offset = 60*off;

            return match;
        }
    }

    if (match_string(date, "PM") == 2) {
        tm->tm_hour = (tm->tm_hour % 12) + 12;
        return 2;
    }

    if (match_string(date, "AM") == 2) {
        tm->tm_hour = (tm->tm_hour % 12) + 0;
        return 2;
    }

    /* BAD CRAP */
    return skip_alpha(date);
}

static int set_date(int year, int month, int day, struct tm *now_tm, time_t now, struct tm *tm)
{
    if (month > 0 && month < 13 && day > 0 && day < 32) {
        struct tm check = *tm;
        struct tm *r = (now_tm ? &check : tm);
        time_t specified;

        r->tm_mon = month - 1;
        r->tm_mday = day;
        if (year == -1) {
            if (!now_tm)
                return 1;
            r->tm_year = now_tm->tm_year;
        }
        else if (year >= 1970 && year < 2100)
            r->tm_year = year - 1900;
        else if (year > 70 && year < 100)
            r->tm_year = year;
        else if (year < 38)
            r->tm_year = year + 100;
        else
            return -1;
        if (!now_tm)
            return 0;

        specified = tm_to_time_t(r);

        /* Be it commit time or author time, it does not make
         * sense to specify timestamp way into the future.  Make
         * sure it is not later than ten days from now...
         */
        if ((specified != -1) && (now + 10*24*3600 < specified))
            return -1;
        tm->tm_mon = r->tm_mon;
        tm->tm_mday = r->tm_mday;
        if (year != -1)
            tm->tm_year = r->tm_year;
        return 0;
    }
    return -1;
}

static int set_time(long hour, long minute, long second, struct tm *tm)
{
    /* We accept 61st second because of leap second */
    if (0 <= hour && hour <= 24 &&
        0 <= minute && minute < 60 &&
        0 <= second && second <= 60) {
        tm->tm_hour = hour;
        tm->tm_min = minute;
        tm->tm_sec = second;
        return 0;
    }
    return -1;
}

static int is_date_known(struct tm *tm)
{
    return tm->tm_year != -1 && tm->tm_mon != -1 && tm->tm_mday != -1;
}

static int match_multi_number(timestamp_t num, char c, const char *date,
                  char *end, struct tm *tm, time_t now)
{
    struct tm now_tm;
    struct tm *refuse_future;
    long num2, num3;

    num2 = strtol(end+1, &end, 10);
    num3 = -1;
    if (*end == c && isdigit(((unsigned char *)end)[1]))
        num3 = strtol(end+1, &end, 10);

    /* Time? Date? */
    switch (c) {
    case ':':
        if (num3 < 0)
            num3 = 0;
        if (set_time(num, num2, num3, tm) == 0) {
            /*
             * If %H:%M:%S was just parsed followed by: .<num4>
             * Consider (& discard) it as fractional second
             * if %Y%m%d is parsed before.
             */
            if (*end == '.' && isdigit(((unsigned char *)end)[1]) && is_date_known(tm))
                strtol(end + 1, &end, 10);
            break;
        }
        return 0;

    case '-':
    case '/':
    case '.':
        if (!now)
            now = time(NULL);
        refuse_future = NULL;

#ifdef WIN32
            if (gmtime_s(&now_tm, &now))
#else
            if (gmtime_r(&now, &now_tm))
#endif
            refuse_future = &now_tm;

        if (num > 70) {
            /* yyyy-mm-dd? */
            if (set_date(num, num2, num3, NULL, now, tm) == 0)
                break;
            /* yyyy-dd-mm? */
            if (set_date(num, num3, num2, NULL, now, tm) == 0)
                break;
        }
        /* Our eastern European friends say dd.mm.yy[yy]
         * is the norm there, so giving precedence to
         * mm/dd/yy[yy] form only when separator is not '.'
         */
        if (c != '.' &&
            set_date(num3, num, num2, refuse_future, now, tm) == 0)
            break;
        /* European dd.mm.yy[yy] or funny US dd/mm/yy[yy] */
        if (set_date(num3, num2, num, refuse_future, now, tm) == 0)
            break;
        /* Funny European mm.dd.yy */
        if (c == '.' &&
            set_date(num3, num, num2, refuse_future, now, tm) == 0)
            break;
        return 0;
    }
    return end - date;
}

/*
 * Have we filled in any part of the time/date yet?
 * We just do a binary 'and' to see if the sign bit
 * is set in all the values.
 */
static inline int nodate(struct tm *tm)
{
    return (tm->tm_year &
        tm->tm_mon &
        tm->tm_mday &
        tm->tm_hour &
        tm->tm_min &
        tm->tm_sec) < 0;
}

/*
 * We've seen a digit. Time? Year? Date?
 */
static int match_digit(const char *date, struct tm *tm, int *offset, int *tm_gmt)
{
    int n;
    char *end;
    timestamp_t num;

    num = parse_timestamp(date, &end, 10);

    /*
     * Seconds since 1970? We trigger on that for any numbers with
     * more than 8 digits. This is because we don't want to rule out
     * numbers like 20070606 as a YYYYMMDD date.
     */
    if (num >= 100000000 && nodate(tm)) {
        time_t time = num;

#ifdef WIN32
        if (gmtime_s(tm, &time)) {
#else
        if (gmtime_r(&time, tm)) {
#endif
            *tm_gmt = 1;
            return end - date;
        }
    }

    /*
     * Check for special formats: num[-.:/]num[same]num
     */
    switch (*end) {
    case ':':
    case '.':
    case '/':
    case '-':
        if (isdigit(((unsigned char *)end)[1])) {
            int match = match_multi_number(num, *end, date, end, tm, 0);
            if (match)
                return match;
        }
    }

    /*
     * None of the special formats? Try to guess what
     * the number meant. We use the number of digits
     * to make a more educated guess..
     */
    n = 0;
    do {
        n++;
    } while (isdigit(((unsigned char *)date)[n]));

    /* 8 digits, compact style of ISO-8601's date: YYYYmmDD */
    /* 6 digits, compact style of ISO-8601's time: HHMMSS */
    if (n == 8 || n == 6) {
        unsigned int num1 = num / 10000;
        unsigned int num2 = (num % 10000) / 100;
        unsigned int num3 = num % 100;
        if (n == 8)
            set_date(num1, num2, num3, NULL, time(NULL), tm);
        else if (n == 6 && set_time(num1, num2, num3, tm) == 0 &&
             *end == '.' && isdigit(((unsigned char *)end)[1]))
            strtoul(end + 1, &end, 10);
        return end - date;
    }

    /* Four-digit year or a timezone? */
    if (n == 4) {
        if (num <= 1400 && *offset == -1) {
            unsigned int minutes = num % 100;
            unsigned int hours = num / 100;
            *offset = hours*60 + minutes;
        } else if (num > 1900 && num < 2100)
            tm->tm_year = num - 1900;
        return n;
    }

    /*
     * Ignore lots of numerals. We took care of 4-digit years above.
     * Days or months must be one or two digits.
     */
    if (n > 2)
        return n;

    /*
     * NOTE! We will give precedence to day-of-month over month or
     * year numbers in the 1-12 range. So 05 is always "mday 5",
     * unless we already have a mday..
     *
     * IOW, 01 Apr 05 parses as "April 1st, 2005".
     */
    if (num > 0 && num < 32 && tm->tm_mday < 0) {
        tm->tm_mday = num;
        return n;
    }

    /* Two-digit year? */
    if (n == 2 && tm->tm_year < 0) {
        if (num < 10 && tm->tm_mday >= 0) {
            tm->tm_year = num + 100;
            return n;
        }
        if (num >= 70) {
            tm->tm_year = num;
            return n;
        }
    }

    if (num > 0 && num < 13 && tm->tm_mon < 0)
        tm->tm_mon = num-1;

    return n;
}

static int match_tz(const char *date, int *offp)
{
    char *end;
    int hour = strtoul(date + 1, &end, 10);
    int n = end - (date + 1);
    int min = 0;

    if (n == 4) {
        /* hhmm */
        min = hour % 100;
        hour = hour / 100;
    } else if (n != 2) {
        min = 99; /* random crap */
    } else if (*end == ':') {
        /* hh:mm? */
        min = strtoul(end + 1, &end, 10);
        if (end - (date + 1) != 5)
            min = 99; /* random crap */
    } /* otherwise we parsed "hh" */

    /*
     * Don't accept any random crap. Even though some places have
     * offset larger than 12 hours (e.g. Pacific/Kiritimati is at
     * UTC+14), there is something wrong if hour part is much
     * larger than that. We might also want to check that the
     * minutes are divisible by 15 or something too. (Offset of
     * Kathmandu, Nepal is UTC+5:45)
     */
    if (min < 60 && hour < 24) {
        int offset = hour * 60 + min;
        if (*date == '-')
            offset = -offset;
        *offp = offset;
    }
    return end - date;
}

static void date_string(timestamp_t date, int offset, char *buf, int bufsize)
{
    int sign = '+';

    if (offset < 0) {
        offset = -offset;
        sign = '-';
    }
    snprintf(buf, bufsize, "%"PRItime" %c%02d%02d", date, sign, offset / 60, offset % 60);
}

/*
 * Parse a string like "0 +0000" as ancient timestamp near epoch, but
 * only when it appears not as part of any other string.
 */
static int match_object_header_date(const char *date, timestamp_t *timestamp, int *offset)
{
    char *end;
    timestamp_t stamp;
    int ofs;

    if (*date < '0' || '9' < *date)
        return -1;
    stamp = parse_timestamp(date, &end, 10);
    if (*end != ' ' || stamp == TIME_MAX || (end[1] != '+' && end[1] != '-'))
        return -1;
    date = end + 2;
    ofs = strtol(date, &end, 10);
    if ((*end != '\0' && (*end != '\n')) || end != date + 4)
        return -1;
    ofs = (ofs / 100) * 60 + (ofs % 100);
    if (date[-1] == '-')
        ofs = -ofs;
    *timestamp = stamp;
    *offset = ofs;
    return 0;
}

/* Gr. strptime is crap for this; it doesn't have a way to require RFC2822
   (i.e. English) day/month names, and it doesn't work correctly with %z. */
int parse_date_basic(const char *date, timestamp_t *timestamp, int *offset)
{
    struct tm tm;
    int tm_gmt;
    timestamp_t dummy_timestamp;
    int dummy_offset;

    if (!timestamp)
        timestamp = &dummy_timestamp;
    if (!offset)
        offset = &dummy_offset;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = -1;
    tm.tm_mon = -1;
    tm.tm_mday = -1;
    tm.tm_isdst = -1;
    tm.tm_hour = -1;
    tm.tm_min = -1;
    tm.tm_sec = -1;
    *offset = -1;
    tm_gmt = 0;

    if (*date == '@' &&
        !match_object_header_date(date + 1, timestamp, offset))
        return 0; /* success */
    for (;;) {
        int match = 0;
        unsigned char c = *date;

        /* Stop at end of string or newline */
        if (!c || c == '\n')
            break;

        if (isalpha(c))
            match = match_alpha(date, &tm, offset);
        else if (isdigit(c))
            match = match_digit(date, &tm, offset, &tm_gmt);
        else if ((c == '-' || c == '+') && isdigit(((unsigned char *)date)[1]))
            match = match_tz(date, offset);

        if (!match) {
            /* BAD CRAP */
            match = 1;
        }

        date += match;
    }

    /* do not use mktime(), which uses local timezone, here */
    *timestamp = tm_to_time_t(&tm);
    if (*timestamp == -1)
        return -1;

    if (*offset == -1) {
        time_t temp_time;

        /* gmtime_r() in match_digit() may have clobbered it */
        tm.tm_isdst = -1;
        temp_time = mktime(&tm);
        if ((time_t)*timestamp > temp_time) {
            *offset = ((time_t)*timestamp - temp_time) / 60;
        } else {
            *offset = -(int)((temp_time - (time_t)*timestamp) / 60);
        }
    }

    if (!tm_gmt)
        *timestamp -= *offset * 60;
    return 0; /* success */
}

int parse_expiry_date(const char *date, timestamp_t *timestamp)
{
    int errors = 0;

    if (!strcmp(date, "never") || !strcmp(date, "false"))
        *timestamp = 0;
    else if (!strcmp(date, "all") || !strcmp(date, "now"))
        /*
         * We take over "now" here, which usually translates
         * to the current timestamp.  This is because the user
         * really means to expire everything that was done in
         * the past, and by definition reflogs are the record
         * of the past, and there is nothing from the future
         * to be kept.
         */
        *timestamp = TIME_MAX;
    else
        *timestamp = approxidate_careful(date, &errors);

    return errors;
}

int parse_date(const char *date, char *result, int resultsize)
{
    timestamp_t timestamp;
    int offset;
    if (parse_date_basic(date, &timestamp, &offset))
        return -1;
    date_string(timestamp, offset, result, resultsize);
    return 0;
}

static enum date_mode_type parse_date_type(const char *format, const char **end)
{
    if (skip_prefix(format, "relative", end))
        return DATE_RELATIVE;
    if (skip_prefix(format, "iso8601-strict", end) ||
        skip_prefix(format, "iso-strict", end))
        return DATE_ISO8601_STRICT;
    if (skip_prefix(format, "iso8601", end) ||
        skip_prefix(format, "iso", end))
        return DATE_ISO8601;
    if (skip_prefix(format, "rfc2822", end) ||
        skip_prefix(format, "rfc", end))
        return DATE_RFC2822;
    if (skip_prefix(format, "short", end))
        return DATE_SHORT;
    if (skip_prefix(format, "default", end))
        return DATE_NORMAL;
    if (skip_prefix(format, "raw", end))
        return DATE_RAW;
    if (skip_prefix(format, "unix", end))
        return DATE_UNIX;
    if (skip_prefix(format, "format", end))
        return DATE_STRFTIME;

    trace_msg("unknown date format %s", format);
    return DATE_NORMAL;
}

void parse_date_format(const char *format, struct date_mode *mode)
{
    const char *p;

    /* "auto:foo" is "if tty/pager, then foo, otherwise normal" */
    if (skip_prefix(format, "auto:", &p)) {
        if (isatty(1))
            format = p;
        else
            format = "default";
    }

    /* historical alias */
    if (!strcmp(format, "local"))
        format = "default-local";

    mode->type = parse_date_type(format, &p);
    mode->local = 0;

    if (skip_prefix(p, "-local", &p))
        mode->local = 1;

    if (mode->type == DATE_STRFTIME) {
        if (!skip_prefix(p, ":", &p))
            trace_msg("date format missing colon separator: %s", format);
        snprintf(mode->strftime_fmt, sizeof(mode->strftime_fmt), "%s", p);
    } else if (*p)
        trace_msg("unknown date format %s", format);
}

void datestamp(char *out, int outsize)
{
    time_t now;
    int offset;
    struct tm tm = { 0 };

    time(&now);

#ifdef WIN32
    localtime_s(&tm, &now);
    offset = tm_to_time_t(&tm) - now;
#else
    offset = tm_to_time_t(localtime_r(&now, &tm)) - now;
#endif
    offset /= 60;

    date_string(now, offset, out, outsize);
}

/*
 * Relative time update (eg "2 days ago").  If we haven't set the time
 * yet, we need to set it from current time.
 */
static time_t update_tm(struct tm *tm, struct tm *now, time_t sec)
{
    time_t n;

    if (tm->tm_mday < 0)
        tm->tm_mday = now->tm_mday;
    if (tm->tm_mon < 0)
        tm->tm_mon = now->tm_mon;
    if (tm->tm_year < 0) {
        tm->tm_year = now->tm_year;
        if (tm->tm_mon > now->tm_mon)
            tm->tm_year--;
    }
    n = mktime(tm) - sec;
#ifdef WIN32
    localtime_s(tm, &n);
#else
    localtime_r(&n, tm);
#endif
    return n;
}

/*
 * Do we have a pending number at the end, or when
 * we see a new one? Let's assume it's a month day,
 * as in "Dec 6, 1992"
 */
static void pending_number(struct tm *tm, int *num)
{
    int number = *num;

    if (number) {
        *num = 0;
        if (tm->tm_mday < 0 && number < 32)
            tm->tm_mday = number;
        else if (tm->tm_mon < 0 && number < 13)
            tm->tm_mon = number-1;
        else if (tm->tm_year < 0) {
            if (number > 1969 && number < 2100)
                tm->tm_year = number - 1900;
            else if (number > 69 && number < 100)
                tm->tm_year = number;
            else if (number < 38)
                tm->tm_year = 100 + number;
            /* We screw up for number = 00 ? */
        }
    }
}

static void date_now(struct tm *tm, struct tm *now, int *num)
{
    *num = 0;
    update_tm(tm, now, 0);
}

static void date_yesterday(struct tm *tm, struct tm *now, int *num)
{
    *num = 0;
    update_tm(tm, now, 24*60*60);
}

static void date_time(struct tm *tm, struct tm *now, int hour)
{
    if (tm->tm_hour < hour)
        update_tm(tm, now, 24*60*60);
    tm->tm_hour = hour;
    tm->tm_min = 0;
    tm->tm_sec = 0;
}

static void date_midnight(struct tm *tm, struct tm *now, int *num)
{
    pending_number(tm, num);
    date_time(tm, now, 0);
}

static void date_noon(struct tm *tm, struct tm *now, int *num)
{
    pending_number(tm, num);
    date_time(tm, now, 12);
}

static void date_tea(struct tm *tm, struct tm *now, int *num)
{
    pending_number(tm, num);
    date_time(tm, now, 17);
}

static void date_pm(struct tm *tm, struct tm *now, int *num)
{
    int hour, n = *num;
    *num = 0;

    hour = tm->tm_hour;
    if (n) {
        hour = n;
        tm->tm_min = 0;
        tm->tm_sec = 0;
    }
    tm->tm_hour = (hour % 12) + 12;
}

static void date_am(struct tm *tm, struct tm *now, int *num)
{
    int hour, n = *num;
    *num = 0;

    hour = tm->tm_hour;
    if (n) {
        hour = n;
        tm->tm_min = 0;
        tm->tm_sec = 0;
    }
    tm->tm_hour = (hour % 12);
}

static void date_never(struct tm *tm, struct tm *now, int *num)
{
    time_t n = 0;
#ifdef WIN32
    localtime_s(tm, &n);
#else
    localtime_r(&n, tm);
#endif
    *num = 0;
}

static const struct special {
    const char *name;
    void (*fn)(struct tm *, struct tm *, int *);
} special[] = {
    { "yesterday", date_yesterday },
    { "noon", date_noon },
    { "midnight", date_midnight },
    { "tea", date_tea },
    { "PM", date_pm },
    { "AM", date_am },
    { "never", date_never },
    { "now", date_now },
    { NULL }
};

static const char *number_name[] = {
    "zero", "one", "two", "three", "four",
    "five", "six", "seven", "eight", "nine", "ten",
};

static const struct typelen {
    const char *type;
    int length;
} typelen[] = {
    { "seconds", 1 },
    { "minutes", 60 },
    { "hours", 60*60 },
    { "days", 24*60*60 },
    { "weeks", 7*24*60*60 },
    { NULL }
};

static const char *approxidate_alpha(const char *date, struct tm *tm, struct tm *now, int *num, int *touched)
{
    const struct typelen *tl;
    const struct special *s;
    const char *end = date;
    int i;

    while (isalpha(*((unsigned char *)(++end))))
        ;

    for (i = 0; i < 12; i++) {
        int match = match_string(date, month_names[i]);
        if (match >= 3) {
            tm->tm_mon = i;
            *touched = 1;
            return end;
        }
    }

    for (s = special; s->name; s++) {
        int len = strlen(s->name);
        if (match_string(date, s->name) == len) {
            s->fn(tm, now, num);
            *touched = 1;
            return end;
        }
    }

    if (!*num) {
        for (i = 1; i < 11; i++) {
            int len = strlen(number_name[i]);
            if (match_string(date, number_name[i]) == len) {
                *num = i;
                *touched = 1;
                return end;
            }
        }
        if (match_string(date, "last") == 4) {
            *num = 1;
            *touched = 1;
        }
        return end;
    }

    tl = typelen;
    while (tl->type) {
        int len = strlen(tl->type);
        if (match_string(date, tl->type) >= len-1) {
            update_tm(tm, now, tl->length * *num);
            *num = 0;
            *touched = 1;
            return end;
        }
        tl++;
    }

    for (i = 0; i < 7; i++) {
        int match = match_string(date, weekday_names[i]);
        if (match >= 3) {
            int diff, n = *num -1;
            *num = 0;

            diff = tm->tm_wday - i;
            if (diff <= 0)
                n++;
            diff += 7*n;

            update_tm(tm, now, diff * 24 * 60 * 60);
            *touched = 1;
            return end;
        }
    }

    if (match_string(date, "months") >= 5) {
        int n;
        update_tm(tm, now, 0); /* fill in date fields if needed */
        n = tm->tm_mon - *num;
        *num = 0;
        while (n < 0) {
            n += 12;
            tm->tm_year--;
        }
        tm->tm_mon = n;
        *touched = 1;
        return end;
    }

    if (match_string(date, "years") >= 4) {
        update_tm(tm, now, 0); /* fill in date fields if needed */
        tm->tm_year -= *num;
        *num = 0;
        *touched = 1;
        return end;
    }

    return end;
}

static const char *approxidate_digit(const char *date, struct tm *tm, int *num,
                     time_t now)
{
    char *end;
    timestamp_t number = parse_timestamp(date, &end, 10);

    switch (*end) {
    case ':':
    case '.':
    case '/':
    case '-':
        if (isdigit(((unsigned char *)end)[1])) {
            int match = match_multi_number(number, *end, date, end,
                               tm, now);
            if (match)
                return date + match;
        }
    }

    /* Accept zero-padding only for small numbers ("Dec 02", never "Dec 0002") */
    if (date[0] != '0' || end - date <= 2)
        *num = number;
    return end;
}

static timestamp_t approxidate_str(
    const char *date,
    time_t time_sec,
    int *error_ret)
{
    int number = 0;
    int touched = 0;
    struct tm tm, now;

#ifdef WIN32
    localtime_s(&tm, &time_sec);
#else
    localtime_r(&time_sec, &tm);
#endif



    now = tm;

    tm.tm_year = -1;
    tm.tm_mon = -1;
    tm.tm_mday = -1;

    for (;;) {
        unsigned char c = *date;
        if (!c)
            break;
        date++;
        if (isdigit(c)) {
            pending_number(&tm, &number);
            date = approxidate_digit(date-1, &tm, &number, time_sec);
            touched = 1;
            continue;
        }
        if (isalpha(c))
            date = approxidate_alpha(date-1, &tm, &now, &number, &touched);
    }
    pending_number(&tm, &number);
    if (!touched)
        *error_ret = 1;
    return (timestamp_t)update_tm(&tm, &now, 0);
}

timestamp_t approxidate_relative(const char *date)
{
    time_t tv;
    timestamp_t timestamp;
    int offset;
    int errors = 0;

    if (!parse_date_basic(date, &timestamp, &offset))
        return timestamp;

    time(&tv);
    return approxidate_str(date, tv, &errors);
}

timestamp_t approxidate_careful(const char *date, int *error_ret)
{
    time_t tv;
    timestamp_t timestamp;
    int offset;
    int dummy = 0;
    if (!error_ret)
        error_ret = &dummy;

    if (!parse_date_basic(date, &timestamp, &offset)) {
        *error_ret = 0;
        return timestamp;
    }

    time(&tv);
    return approxidate_str(date, tv, error_ret);
}

int date_overflows(timestamp_t t)
{
    time_t sys;

    /* If we overflowed our timestamp data type, that's bad... */
    if ((uintmax_t)t >= TIME_MAX)
        return 1;

    /*
     * ...but we also are going to feed the result to system
     * functions that expect time_t, which is often "signed long".
     * Make sure that we fit into time_t, as well.
     */
    sys = t;
    return t != sys || (t < 1) != (sys < 1);
}
