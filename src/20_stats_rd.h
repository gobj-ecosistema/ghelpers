/****************************************************************************
 *          STATS_RD.H
 *
 *          Simple statistics handler, reader
 *
 *          Copyright (c) 2018 Niyamaka.
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
    Get `metrics`

    Return example:
    {
        "flow_rate": {
            "last_week_in_seconds": {
                "period": "WDAY",
                "units": "SEC",
                "compute": "",
                "real": true,
                "data": [
                    {
                        "fr_t": 62553600,
                        "to_t": 62639999,
                        "fr_d": "1971-12-26T00:00:00.0+0000",
                        "to_d": "1971-12-26T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/last_week_in_seconds-7.dat"
                    },
                    {
                        "fr_t": 62640000,
                        "to_t": 62726399,
                        "fr_d": "1971-12-27T00:00:00.0+0000",
                        "to_d": "1971-12-27T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/last_week_in_seconds-1.dat"
                    },
                    {
                        "fr_t": 62726400,
                        "to_t": 62812799,
                        "fr_d": "1971-12-28T00:00:00.0+0000",
                        "to_d": "1971-12-28T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/last_week_in_seconds-2.dat"
                    },
                    {
                        "fr_t": 62812800,
                        "to_t": 62899199,
                        "fr_d": "1971-12-29T00:00:00.0+0000",
                        "to_d": "1971-12-29T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/last_week_in_seconds-3.dat"
                    },
                    {
                        "fr_t": 62899200,
                        "to_t": 62985599,
                        "fr_d": "1971-12-30T00:00:00.0+0000",
                        "to_d": "1971-12-30T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/last_week_in_seconds-4.dat"
                    },
                    {
                        "fr_t": 62985600,
                        "to_t": 63071999,
                        "fr_d": "1971-12-31T00:00:00.0+0000",
                        "to_d": "1971-12-31T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/last_week_in_seconds-5.dat"
                    },
                    {
                        "fr_t": 63072000,
                        "to_t": 63072000,
                        "fr_d": "1972-01-01T00:00:00.0+0000",
                        "to_d": "1972-01-01T00:00:00.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/last_week_in_seconds-6.dat"
                    }
                ]
            },
            "years_in_hours": {
                "period": "YEAR",
                "units": "HOUR",
                "compute": "",
                "real": true,
                "data": [
                    {
                        "fr_t": 3599,
                        "to_t": 31535999,
                        "fr_d": "1970-01-01T00:59:59.0+0000",
                        "to_d": "1970-12-31T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/years_in_hours-1970.dat"
                    },
                    {
                        "fr_t": 31539599,
                        "to_t": 63071999,
                        "fr_d": "1971-01-01T00:59:59.0+0000",
                        "to_d": "1971-12-31T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/years_in_hours-1971.dat"
                    }
                ]
            },
            "years_in_days": {
                "period": "YEAR",
                "units": "YDAY",
                "compute": "",
                "real": true,
                "data": [
                    {
                        "fr_t": 86399,
                        "to_t": 31535999,
                        "fr_d": "1970-01-01T23:59:59.0+0000",
                        "to_d": "1970-12-31T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/years_in_days-1970.dat"
                    },
                    {
                        "fr_t": 31622399,
                        "to_t": 63071999,
                        "fr_d": "1971-01-01T23:59:59.0+0000",
                        "to_d": "1971-12-31T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/years_in_days-1971.dat"
                    }
                ]
            },
            "years_in_months": {
                "period": "YEAR",
                "units": "MON",
                "compute": "",
                "real": true,
                "data": [
                    {
                        "fr_t": 2678399,
                        "to_t": 31535999,
                        "fr_d": "1970-01-31T23:59:59.0+0000",
                        "to_d": "1970-12-31T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/years_in_months-1970.dat"
                    },
                    {
                        "fr_t": 34214399,
                        "to_t": 63071999,
                        "fr_d": "1971-01-31T23:59:59.0+0000",
                        "to_d": "1971-12-31T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/years_in_months-1971.dat"
                    }
                ]
            },
            "years_in_years": {
                "period": "years",
                "units": "YEAR",
                "compute": "",
                "real": true,
                "data": [
                    {
                        "fr_t": 31535999,
                        "to_t": 63071999,
                        "fr_d": "1970-12-31T23:59:59.0+0000",
                        "to_d": "1971-12-31T23:59:59.0+0000",
                        "file": "/yuneta/store/stats/test_stats/flow_rate/years_in_years-years.dat"
                    }
                ]
            }
        },
        "velocity": {}
    }

**rst**/
PUBLIC json_t *rstats_metrics( // Return must be used in below functions
    json_t *stats
);

/**rst**
    List variables of `metrics`

    Return example:

    [
        "flow_rate",
        "velocity"
    ]

**rst**/
PUBLIC json_t *rstats_list_variables(
    json_t *metrics
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
    json_t *metrics,
    const char *variable
);

/**rst**
    Find `metric` by units
**rst**/
PUBLIC json_t *find_metric_by_units(
    json_t *metrics,
    const char *variable,
    const char *units,
    BOOL verbose
);

/**rst**
    Get `metric`, find by units else find by metric_name
**rst**/
PUBLIC json_t *rstats_metric(
    json_t *metrics,
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
