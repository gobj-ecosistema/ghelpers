/****************************************************************************
 *          STATS_WR.H
 *
 *          Simple statistics handler, writer
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "02_dirs.h"
#include "14_kw.h"
#include "20_stats.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/

/**rst**
   Open simple stats, as master. Only master can write.

   Config:
        path:                   string, directory of stats data.
        xpermission:            int,  Used in directory creation, default 02770
        rpermission:            int,  Used in file creation, default 0660
        on_critical_error:      int,  default "0" LOG_NONE

**rst**/
PUBLIC json_t *wstats_open(
    json_t *jn_stats  // owned
);

/**rst**
    Masks:
        "SEC"   "%S"    Seconds         [00-60] (1 leap second)
        "MIN"   "%M"    Minutes         [00-59]
        "HOUR"  "%H"    Hours           [00-23]
        "MDAY"  "%d"    Day             [01-31]
        "MON"   "%m"    Month           [01-12]
        "YEAR"  "%Y"    Year
        "WDAY"  "%u"    Day of week     [1-7]
        "YDAY"  "%j"    Days in year    [001-366]
        "CENT"  "%C"    The century     (year/100) as a 2-digit integer);

   Add new metric

   jn_metric:           dict
        metric_name:        string, metric name
        version:            string/numeric, metric version
        group:              string, optional, to group metrics in group directory.
        types:              key: list of dicts
            [
                {
                    id:             string, metric type id
                    metric_type:    string, ["average", "min", "max", "sum"], default: ""
                    value_type:     json_real or json_integer
                    filename_mask   string, mask of file
                    time_mask       string, mask of time
                }
            ]
**rst**/
PUBLIC json_t *wstats_add_metric( // Return is NOT YOURS
    json_t *stats,
    json_t *jn_metric  // owned
);

/**rst**
   Close simple stats
**rst**/
PUBLIC void wstats_close(
    json_t *stats
);

/**rst**
   Add metric value
**rst**/
PUBLIC void wstats_add_value(
    json_t *stats,
    const char *metric_name,
    const char *group,
    time_t t,   // UTC time
    json_t *jn_value
);

/**rst**
   Save partial data
**rst**/
PUBLIC int wstats_save(
    json_t *stats
);

/**rst**
   Restore partial data
**rst**/
PUBLIC int wstats_restore(
    json_t *stats
);


#ifdef __cplusplus
}
#endif
