/****************************************************************************
 *          STATS_RD.H
 *
 *          Simple statistics handler, reader
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <inttypes.h>
#include "20_stats_wr.h"
#include "20_stats_rd.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Structures
 ***************************************************************/
typedef union stats_value_s {
    int64_t i64;        // signed integer 64 bits
    double d;           // double
} stats_value_t;

typedef struct stats_record_s {
    uint64_t t;
    stats_value_t v;
} stats_record_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC const char *filtra_time_mask(const char *mask); // See 20_stats_wr.h for masks

#ifdef __cplusplus
}
#endif
