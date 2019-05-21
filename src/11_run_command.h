/****************************************************************************
 *          RUN_EXECUTABLE.H
 *          Run a executable
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/

#ifndef ___RUN_EXECUTABLE_H__
#define ___RUN_EXECUTABLE_H__ 1

/*
 *  Dependencies
 */
#include <uv.h>
#include "10_glogger.h"

#ifdef __cplusplus
extern "C"{
#endif

PUBLIC int run_command(  // use popen(), synchronous
    const char *command,
    char *bf,   // buffer to put the output of command
    size_t bfsize
);

PUBLIC int run_process2(  // use fork(), synchronous
    const char *path, char *const argv[]
);

#ifdef __cplusplus
}
#endif


#endif

