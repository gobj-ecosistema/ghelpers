/****************************************************************************
 *          STATS_RD.H
 *
 *          Simple statistics handler, reader
 *
 *          Copyright (c) 2018,2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/

#pragma once

#include "11_gbmem.h"
#include "12_walkdir.h"
#include "14_kw.h"
#include "20_stats.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/

/**rst**
   Open simple stats, as client. Only read.

   Config:
        path:                   string, directory of stats data.
        groups:                 string, [string,], optional, read metric belong to groups.

**rst**/
PUBLIC json_t *rstats_open(
    json_t *jn_stats  // owned
);

/**rst**
   Close simple stats
**rst**/
PUBLIC void rstats_close(
    json_t *stats
);

/**rst**
    Get `variables`

    Return example:
    {
        "queues.dbwriter^10120.dbwrite-queue-9": {
            "last_week_in_seconds": {
                "metric_id": "last_week_in_seconds",
                "variable": "queues.dbwriter^10120.dbwrite-queue-9",
                "period": "WDAY",
                "units": "SEC",
                "compute": "",
                "real": true,
                "data": [
                    {
                        "fr_t": 1577093728,
                        "to_t": 1577098191,
                        "fr_d": "2019-12-23T09:35:28.0+0000",
                        "to_d": "2019-12-23T10:49:51.0+0000",
                        "file": "/yuneta/store/stats/gpss/dbwriter^10120/queues/dbwrite-queue-9/last_week_in_seconds-1.dat"
                    }
                ]
            }
        },
        ...
    }

**rst**/
PUBLIC json_t *rstats_variables( // Return must be used in below functions
    json_t *stats
);

/**rst**
    List variables of `variables`

    Return example:

    [
        "flow_rate",
        "velocity"
    ]

**rst**/
PUBLIC json_t *rstats_list_variables(
    json_t *variables
);

/**rst**
    List units and limits of variable.
    WARNING Decoupled return.

    Return example:

    With `variable` empty:
    {
        "flow_rate": {
            "SEC": [
                62553600,
                63072000
            ],
            "HOUR": [
                3599,
                63071999
            ],
            "YDAY": [
                86399,
                63071999
            ],
            "MON": [
                2678399,
                63071999
            ],
            "YEAR": [
                31535999,
                63071999
            ]
        },
        "velocity": {}
    }

    With bad `variable` empty:
    {}

    With good `variable`:
    {
        "SEC": [
            62553600,
            63072000
        ],
        "HOUR": [
            3599,
            63071999
        ],
        "YDAY": [
            86399,
            63071999
        ],
        "MON": [
            2678399,
            63071999
        ],
        "YEAR": [
            31535999,
            63071999
        ]
    }


**rst**/
PUBLIC json_t *rstats_list_limits(
    json_t *variables,
    const char *variable
);

/**rst**
    Find `metric` by units
**rst**/
PUBLIC json_t *find_metric_by_units(
    json_t *variables,
    const char *variable,
    const char *units,
    BOOL verbose
);

/**rst**
    Get `metric`, find by units else find by metric_name
**rst**/
PUBLIC json_t *rstats_metric(
    json_t *variables,
    const char *variable,
    const char *metric_name,
    const char *units,
    BOOL verbose
);

/**rst**
    Get stats of metric of time range.
**rst**/
PUBLIC json_t *rstats_get_data(
    json_t *metric,
    uint64_t from_t,
    uint64_t to_t
);

#ifdef __cplusplus
}
#endif
