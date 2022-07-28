/****************************************************************************
 *          REFERENCE.H
 *          Get references
 *
 *          Copyright (c) 2014 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "00_gtypes.h"

#ifdef __cplusplus
extern "C"{
#endif

uint32_t short_reference(void);
uint64_t long_reference(void);

PUBLIC int create_uuid(char *bf, int bfsize);

#ifdef __cplusplus
}
#endif
