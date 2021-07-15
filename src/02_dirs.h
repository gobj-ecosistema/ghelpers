/****************************************************************************
 *              DIRS.H
 *              Copyright (c) 2014 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

/*
 *  Dependencies
 */
#include <stdio.h>
#include "01_print_error.h"
#include "01_gstrings.h"

#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Prototypes
 *****************************************************************/
/*
 *  Wrap mkdir and creat
 *  The use of this functions implies the use of 00_security.h's permission system:
 *  umask will be set to 0 and we control all permission mode.
 */

PUBLIC int newdir(const char *path, int permission);
PUBLIC int newfile(const char *path, int permission, BOOL overwrite);
PUBLIC int open_exclusive(const char *path, int flags, int permission);  // open exclusive

PUBLIC uint64_t filesize(const char *path);
PUBLIC uint64_t filesize2(int fd);

PUBLIC int lock_file(int fd);
PUBLIC int unlock_file(int fd);

PUBLIC BOOL is_regular_file(const char *path);
PUBLIC BOOL is_directory(const char *path);
PUBLIC BOOL file_exists(const char *directory, const char *filename);
PUBLIC BOOL subdir_exists(const char *directory, const char *subdir);
PUBLIC int file_remove(const char *directory, const char *filename);

PUBLIC int mkrdir(const char *path, int index, int permission);
PUBLIC int rmrdir(const char *root_dir);
PUBLIC int rmrcontentdir(const char *root_dir);
PUBLIC int copyfile(
    const char* source,
    const char* destination,
    int permission,
    BOOL overwrite
);

PUBLIC char *get_last_segment(char *path);
PUBLIC char *pop_last_segment(char *path); // WARNING path modified

PUBLIC char *build_path2(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2
);
PUBLIC char *build_path3(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2,
    const char *dir3
);
PUBLIC char *build_path4(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2,
    const char *dir3,
    const char *dir4
);
PUBLIC char *build_path5(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2,
    const char *dir3,
    const char *dir4,
    const char *dir5
);
PUBLIC char *build_path6(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2,
    const char *dir3,
    const char *dir4,
    const char *dir5,
    const char *dir6
);

#ifdef __cplusplus
}
#endif
