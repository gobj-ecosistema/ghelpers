/****************************************************************************
 *              WALKDIR.H
 *              Copyright (c) 2014 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#ifndef _WALKDIR_H
#define _WALKDIR_H

/*
 *  Dependencies
 */
#include "11_gbmem.h"
#include "12_gstrings2.h"

#ifdef __cplusplus
extern "C"{
#endif

/********************************************************************
 *        Structures
 ********************************************************************/
typedef enum {
    WD_RECURSIVE            = 0x0001,   /* traverse all tree */
    WD_HIDDENFILES          = 0x0002,   /* show hidden files too */
    WD_MATCH_DIRECTORY      = 0x0010,   /* if pattern is used on dir names */
    WD_MATCH_REGULAR_FILE   = 0x0020,   /* if pattern is used on regular files */
    WD_MATCH_PIPE           = 0x0040,   /* if pattern is used on pipes */
    WD_MATCH_SYMBOLIC_LINK  = 0x0080,   /* if pattern is used on symbolic links */
    WD_MATCH_SOCKET         = 0x0100,   /* if pattern is used on sockets */
} wd_option;

typedef enum {
    WD_TYPE_DIRECTORY = 1,
    WD_TYPE_REGULAR_FILE,
    WD_TYPE_PIPE,
    WD_TYPE_SYMBOLIC_LINK,
    WD_TYPE_SOCKET,
} wd_found_type;

/********************************************************************
 *        Prototypes
 ********************************************************************/
typedef BOOL (*walkdir_cb)(
    void *user_data,
    wd_found_type type,     // type found
    const char *fullpath,   // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[256]  filename
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
);
/*
 *  Walk directory tree calling callback witch each file found.
 *  If the callback return FALSE then stop traverse tree.
 *  Return standar unix: 0 success, -1 fail
 */
PUBLIC int walk_dir_tree(
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    walkdir_cb cb,
    void *user_data
);

PUBLIC int get_number_of_files(
    const char *root_dir,
    const char *pattern,
    wd_option opt
);

/*
 * Return the ordered full tree filenames of root_dir
 * WARNING free the returned value (char **) with free_ordered_filename_array()
 * NOTICE: Sometimes I reinvent the wheel: use glob() please.
 */
PUBLIC char **get_ordered_filename_array(
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    int *size
);
PUBLIC void free_ordered_filename_array(char **array, int size);


#ifdef __cplusplus
}
#endif

#endif
