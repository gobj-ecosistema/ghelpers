/****************************************************************************
 *              WALKDIR.C
 *              Copyright (c) 2014 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "12_walkdir.h"

/*****************************************************************
 *          Constants
 *****************************************************************/

/*****************************************************************
 *          Structures
 *****************************************************************/

/*****************************************************************
 *          Data
 *****************************************************************/

/*****************************************************************
 *          Prototypes
 *****************************************************************/

/****************************************************************************
 *
 ****************************************************************************/
PRIVATE int _walk_tree(
    const char *root_dir,
    regex_t *reg,
    void *user_data,
    wd_option opt,
    int level,
    walkdir_cb cb)
{
    struct dirent *dent;
    DIR *dir;
    struct stat st;
    wd_found_type type;
    level++;
    int index=0;

    if (!(dir = opendir(root_dir))) {
        // NO saques traza de:
        // EACCES Permission denied (lo dará cuando es un fichero abierto por otro, por ejemplo)
        // ENOENT No such file or directory (links rotos, por ejemplo)
        if(!(errno==EACCES ||errno==ENOENT)) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "opendir() FAILED",
                "path",         "%s", root_dir,
                "error",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
        }
        return -1;
    }

    while ((dent = readdir(dir))) {
        char *dname = dent->d_name;
        if (!strcmp(dname, ".") || !strcmp(dname, ".."))
            continue;
        if (!(opt & WD_HIDDENFILES) && dname[0] == '.')
            continue;

        char *path = NULL;
        if(root_dir[strlen(root_dir)-1] == '/') {
            path = str_concat(root_dir, dname);
        } else {
            path = str_concat3(root_dir, "/", dname);
        }
        if(!path) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "No memory",
                "size",         "%d", strlen(root_dir)+1+strlen(dname),
                NULL
            );
            break;
        }

        if(stat(path, &st) == -1) {
            // NO saques traza de:
            // EACCES Permission denied (lo dará cuando es un fichero abierto por otro, por ejemplo)
            // ENOENT No such file or directory (links rotos, por ejemplo)
            if(!(errno==EACCES ||errno==ENOENT)) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "stat() FAILED",
                    "path",         "%s", path,
                    "error",        "%d", errno,
                    "strerror",     "%s", strerror(errno),
                    NULL
                );
            }
            str_concat_free(path);
            continue;
        }

        type = 0;
        if(S_ISDIR(st.st_mode)) {
            /* recursively follow dirs */
            if((opt & WD_RECURSIVE)) {
                _walk_tree(path, reg, user_data, opt, level, cb);
            }
            if ((opt & WD_MATCH_DIRECTORY)) {
                type = WD_TYPE_DIRECTORY;
            }
        } else if(S_ISREG(st.st_mode)) {
            if((opt & WD_MATCH_REGULAR_FILE)) {
                type = WD_TYPE_REGULAR_FILE;
            }

        } else if(S_ISFIFO(st.st_mode)) {
            if((opt & WD_MATCH_PIPE)) {
                type = WD_TYPE_PIPE;
            }
        } else if(S_ISLNK(st.st_mode)) {
            if((opt & WD_MATCH_SYMBOLIC_LINK)) {
                type = WD_TYPE_SYMBOLIC_LINK;
            }
        } else if(S_ISSOCK(st.st_mode)) {
            if((opt & WD_MATCH_SOCKET)) {
                type = WD_TYPE_SOCKET;
            }
        } else {
            // type not implemented
            type = 0;
        }

        if(type) {
            if (regexec(reg, dname, 0, 0, 0)==0) {
                if(!(cb)(user_data, type, path, root_dir, dname, level, index)) {
                    // returning FALSE: don't want continue traverse
                    break;
                }
                index++;
            }
        }
        str_concat_free(path);
    }
    closedir(dir);
    return 0;
}

/****************************************************************************
 *  Walk directory tree
 *  Se matchea un único pattern a todo lo encontrado.
 ****************************************************************************/
PUBLIC int walk_dir_tree(
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    walkdir_cb cb,
    void *user_data)
{
    regex_t r;

    if(access(root_dir, 0)!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "directory not found",
            "path",         "%s", root_dir,
           NULL
        );
        return -1;
    }
    int ret = regcomp(&r, pattern, REG_EXTENDED | REG_NOSUB);
    if(ret!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "regcomp() FAILED",
            "error",        "%d", ret,
           NULL
        );
        return -1;
    }

    ret = _walk_tree(root_dir, &r, user_data, opt, 0, cb);
    regfree(&r);
    return ret;
}

/****************************************************************************
 *  Get number of files
 ****************************************************************************/
PRIVATE BOOL _nfiles_cb(
    void *user_data,
    wd_found_type type,
    const char *fullpath,
    const char *directory,
    const char *filename,
    int level,
    int index)
{
    int *nfiles = user_data;
    (*nfiles)++;
    return TRUE; // continue traverse tree
}

PUBLIC int get_number_of_files(
    const char *root_dir,
    const char *pattern,
    wd_option opt)
{
    regex_t r;

    if(access(root_dir, 0)!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "directory not found",
            "path",         "%s", root_dir,
           NULL
        );
        return 0;
    }
    int ret = regcomp(&r, pattern, REG_EXTENDED | REG_NOSUB);
    if(ret!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "regcomp() FAILED",
            "error",        "%d", ret,
           NULL
        );
        return -1;
    }
    int nfiles = 0;
    _walk_tree(root_dir, &r, &nfiles, opt, 0, _nfiles_cb);
    regfree(&r);
    return nfiles;
}

/****************************************************************************
 *  Compare function to sort
 ****************************************************************************/
PRIVATE int cmpstringp(const void *p1, const void *p2) {
/*  The actual arguments to this function are "pointers to
 *  pointers to char", but strcmp(3) arguments are "pointers
 *  to char", hence the following cast plus dereference
 */
    return strcmp(* (char * const *) p1, * (char * const *) p2);
}

/****************************************************************************
 *  Return the ordered full tree filenames of root_dir
 *  WARNING free return array with free_ordered_filename_array()
 *  WARNING: here I don't use gbmem functions
 ****************************************************************************/
struct myfiles_s {
    char **files;
    int *idx;
};
PRIVATE BOOL _fill_array_cb(
    void *user_data,
    wd_found_type type,
    const char *fullpath,
    const char *directory,
    const char *filename,
    int level,
    int index)
{
    struct myfiles_s *myfiles = user_data;
    char **files = myfiles->files;
    int idx = *(myfiles->idx);
    int ln = strlen(fullpath);
    char *ptr = malloc(ln+1);
    if(!ptr) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "malloc() FAILED",
            "error",        "%d", errno,
            "strerror",     "%s", strerror(errno),
           NULL
        );
        return FALSE; // don't continue traverse tree
    }
    memcpy(ptr, fullpath, ln);
    ptr[ln] = 0;

    *(files+idx) = ptr;
    (*myfiles->idx)++;
    return TRUE; // continue traverse tree
}
PUBLIC char **get_ordered_filename_array(
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    int *size)
{
    regex_t r;
    if(size) {
        *size = 0; // initial 0
    }

    if(access(root_dir, 0)!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "directory not found",
            "path",         "%s", root_dir,
           NULL
        );
        return 0;
    }

    int ret = regcomp(&r, pattern, REG_EXTENDED | REG_NOSUB);
    if(ret!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "regcomp() FAILED",
            "error",        "%d", ret,
           NULL
        );
        return 0;
    }
    /*--------------------------------------------------------*
     *  Order required, do some preprocessing:
     *  - get nfiles: number of files
     *  - alloc *system* memory for array of nfiles pointers
     *  - fill array with file names
     *  - order the array
     *  - return the array
     *  - the user must free with free_ordered_filename_array()
     *--------------------------------------------------------*/
    int nfiles = get_number_of_files(root_dir, pattern, opt);
    if(!nfiles) {
        regfree(&r);
        return 0;
    }

    int ln = sizeof(char *) * (nfiles+1);
    // TODO check if required too much memory. Avoid use of swap memory.

    char **files = malloc(ln);
    if(!files) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "malloc() FAILED",
            "error",        "%d", errno,
            "strerror",     "%s", strerror(errno),
           NULL
        );
        regfree(&r);
        return 0;
    }
    memset(files, 0, ln);

    /*
     *  Fill the array
     */
    struct myfiles_s myfiles;
    int idx = 0;
    myfiles.files = files;
    myfiles.idx = &idx;

    _walk_tree(root_dir, &r, &myfiles, opt, 0, _fill_array_cb);
    regfree(&r);

    /*
     *  Order the array
     */
    qsort(files, nfiles, sizeof(char *), cmpstringp);

    if(size) {
        *size = nfiles;
    }
    return files;
}

/****************************************************************************
 *  WARNING: here I don't use gbmem functions
 ****************************************************************************/
PUBLIC void free_ordered_filename_array(char **array, int size)
{
    if(!array) {
        return;
    }
    for(int i=0; i<size; i++) {
        char *ptr = *(array+i);
        if(ptr) {
            free(ptr);
        }
    }
    free(array);
}
