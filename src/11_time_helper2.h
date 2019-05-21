/****************************************************************************
 *              TIME_HELPER2.H
 *              Copyright (C) Linus Torvalds, 2005
 *              Copyright (c) Niyamaka, 2018.
 ****************************************************************************/

#ifndef _TIME_HELPER2_H
#define _TIME_HELPER2_H 1

#include <time.h>
#include <stdint.h>

/*
 *  Dependencies
 */
#include "10_glogger.h"

#ifdef __cplusplus
extern "C"{
#endif

/********************************************************************
 *        Constants
 ********************************************************************/
typedef uintmax_t timestamp_t;
#define PRItime PRIuMAX
#define parse_timestamp strtoumax
#define TIME_MAX UINTMAX_MAX

/*
 * ARRAY_SIZE - get the number of elements in a visible array
 *  <at> x: the array whose size you want.
 *
 * This does not work on pointers, or arrays declared as [], or
 * function parameters.  With correct compiler support, such usage
 * will cause a build error (see the build_assert_or_zero macro).
 */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define bitsizeof(x)  (CHAR_BIT * sizeof(x))

#define maximum_signed_value_of_type(a) \
    (INTMAX_MAX >> (bitsizeof(intmax_t) - bitsizeof(a)))

#define maximum_unsigned_value_of_type(a) \
    (UINTMAX_MAX >> (bitsizeof(uintmax_t) - bitsizeof(a)))

/*
 * Signed integer overflow is undefined in C, so here's a helper macro
 * to detect if the sum of two integers will overflow.
 *
 * Requires: a >= 0, typeof(a) equals typeof(b)
 */
#define signed_add_overflows(a, b) \
    ((b) > maximum_signed_value_of_type(a) - (a))

#define unsigned_add_overflows(a, b) \
    ((b) > maximum_unsigned_value_of_type(a) - (a))

/*
 * If the string "str" begins with the string found in "prefix", return 1.
 * The "out" parameter is set to "str + strlen(prefix)" (i.e., to the point in
 * the string right after the prefix).
 *
 * Otherwise, return 0 and leave "out" untouched.
 *
 * Examples:
 *
 *   [extract branch name, fail if not a branch]
 *   if (!skip_prefix(ref, "refs/heads/", &branch)
 *    return -1;
 *
 *   [skip prefix if present, otherwise use whole string]
 *   skip_prefix(name, "refs/heads/", &name);
 */
static inline int skip_prefix(const char *str, const char *prefix,
                  const char **out)
{
    do {
        if (!*prefix) {
            *out = str;
            return 1;
        }
    } while (*str++ == *prefix++);
    return 0;
}


/*****************************************************************
 *     Prototypes
 *****************************************************************/
struct date_mode {
    enum date_mode_type {
        DATE_NORMAL = 0,
        DATE_RELATIVE,
        DATE_SHORT,
        DATE_ISO8601,
        DATE_ISO8601_STRICT,
        DATE_RFC2822,
        DATE_STRFTIME,
        DATE_RAW,
        DATE_UNIX
    } type;
    char strftime_fmt[256];
    int local;
};

/*
 * Convenience helper for passing a constant type, like:
 *
 *   show_date(t, tz, DATE_MODE(NORMAL));
 */
#define DATE_MODE(t) date_mode_from_type(DATE_##t)
struct date_mode *date_mode_from_type(enum date_mode_type type);

const char *show_date(timestamp_t time, int timezone, const struct date_mode *mode);
void show_date_relative(
    timestamp_t time,
    int tz,
    const struct timeval *now,
    char *timebuf,
    int timebufsize
);
int parse_date(
    const char *date,
    char *out,
    int outsize
);
int parse_date_basic(const char *date, timestamp_t *timestamp, int *offset);
int parse_expiry_date(const char *date, timestamp_t *timestamp);
void datestamp(
    char *out,
    int outsize
);

#define approxidate(s) approxidate_careful((s), NULL)
timestamp_t approxidate_careful(const char *, int *);
timestamp_t approxidate_relative(const char *date, const struct timeval *now);
void parse_date_format(const char *format, struct date_mode *mode);
int date_overflows(timestamp_t date);

#ifdef __cplusplus
}
#endif

#endif /* _TIME_HELPER2_H */
