/****************************************************************************
 *              DIRS.H
 *              Copyright (c) 2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#ifndef _LARGEFILE64_SOURCE
  #define _LARGEFILE64_SOURCE
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
  #include <copyfile.h>
#elif defined(__linux__)
  #include <sys/sendfile.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#ifdef WIN32
    #include <direct.h>
    #include <io.h>
    #include <fcntl.h>
    #include <sys\types.h>
    #include <sys\stat.h>
#else
    #include <unistd.h>
    #include <sys/file.h>
    #include <dirent.h>
#endif

#include "02_dirs.h"

/*****************************************************************
 *     Data
 *****************************************************************/
static BOOL umask_cleared = FALSE;

/***************************************************************************
 *  Create a new directory
 *  The use of this functions implies the use of 00_security.h's permission system:
 *  umask will be set to 0 and we control all permission mode.
 ***************************************************************************/
PUBLIC int newdir(const char *path, int permission)
{
    if(!umask_cleared) {
        umask(0);
        umask_cleared = TRUE;
    }
#ifdef WIN32
    return _mkdir(path);
#else
    return mkdir(path, permission);
#endif
}

/***************************************************************************
 *  Create a new file (only to write)
 *  The use of this functions implies the use of 00_security.h's permission system:
 *  umask will be set to 0 and we control all permission mode.
 ***************************************************************************/
PUBLIC int newfile(const char *path, int permission, BOOL overwrite)
{
    int flags = O_CREAT|O_WRONLY|O_LARGEFILE;

    if(!umask_cleared) {
        umask(0);
        umask_cleared = TRUE;
    }

    if(overwrite)
        flags |= O_TRUNC;
    else
        flags |= O_EXCL;
#ifdef WIN32
    return _open(path, flags, _S_IREAD | _S_IWRITE);
#else
    return open(path, flags, permission);
#endif
}

/***************************************************************************
 *  Open a file as exclusive
 ***************************************************************************/
PUBLIC int open_exclusive(const char *path, int flags, int permission)
{
#ifdef WIN32
    if(!flags) {
        flags = O_RDWR|O_LARGEFILE;
    }

    int fp = _open(path, flags);
    // TODO LockFileEx()
    return fp;
#else
    if(!flags) {
        flags = O_RDWR|O_LARGEFILE|O_NOFOLLOW;
    }

    int fp = open(path, flags, permission);
    if(flock(fp, LOCK_EX|LOCK_NB)<0) {
        close(fp);
        return -1;
    }
    return fp;
#endif
}

/***************************************************************************
 *  Get size of file
 ***************************************************************************/
PUBLIC uint64_t filesize(const char *path)
{
#if defined(WIN32)
    struct stat st;
    int ret = stat(path, &st);
    if(ret < 0) {
        return 0;
    }
    uint64_t size = st.st_size;
    return size;
#else
    struct stat64 st;
    int ret = stat64(path, &st);
    if(ret < 0) {
        return 0;
    }
    uint64_t size = st.st_size;
    return size;
#endif
}

/***************************************************************************
 *  Get size of file
 ***************************************************************************/
PUBLIC uint64_t filesize2(int fd)
{
#if defined(WIN32)
    struct stat st;
    int ret = fstat(fd, &st);
    if(ret < 0) {
        return 0;
    }
    uint64_t size = st.st_size;
    return size;
#else
    struct stat64 st;
    int ret = fstat64(fd, &st);
    if(ret < 0) {
        return 0;
    }
    uint64_t size = st.st_size;
    return size;
#endif
}

/*****************************************************************
 *        lock file
 *****************************************************************/
PUBLIC int lock_file(int fd)
{
#if defined(WIN32)
    // TODO LockFileEx()
    return -1;
#else
    struct flock fl;
    if(fd <= 0) {
        return -1;
    }
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    return fcntl(fd, F_SETLKW, &fl);
#endif
}

/*****************************************************************
 *        unlock file
 *****************************************************************/
PUBLIC int unlock_file(int fd)
{
#if defined(WIN32)
    // TODO LockFileEx()
    return -1;
#else
    struct flock fl;
    if(fd <= 0) {
        return -1;
    }
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    return fcntl(fd, F_SETLKW, &fl);
#endif
}

/***************************************************************************
 *  Tell if path is a regular file
 ***************************************************************************/
PUBLIC BOOL is_regular_file(const char *path)
{
    struct stat buf;
    int ret = stat(path, &buf);
    if(ret < 0) {
        return FALSE;
    }
#ifdef WIN32
    return (buf.st_mode & S_IFREG)?TRUE:FALSE;
#else
    return S_ISREG(buf.st_mode)?TRUE:FALSE;
#endif
}

/***************************************************************************
 *  Tell if path is a directory
 ***************************************************************************/
PUBLIC BOOL is_directory(const char *path)
{
    struct stat buf;
    int ret = stat(path, &buf);
    if(ret < 0) {
        return FALSE;
    }
#ifdef WIN32
    return (buf.st_mode & S_IFDIR)?TRUE:FALSE;
#else
    return S_ISDIR(buf.st_mode)?TRUE:FALSE;
#endif
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL file_exists(const char *directory, const char *filename)
{
    char full_path[PATH_MAX];
    build_path2(full_path, sizeof(full_path), directory, filename);

    if(is_regular_file(full_path)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL subdir_exists(const char *directory, const char *subdir)
{
    char full_path[PATH_MAX];
    build_path2(full_path, sizeof(full_path), directory, subdir);

    if(is_directory(full_path)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int file_remove(const char *directory, const char *filename)
{
    char full_path[PATH_MAX];
    build_path2(full_path, sizeof(full_path), directory, filename);

    if(!is_regular_file(full_path)) {
        return -1;
    }
    return unlink(full_path);
}

/***************************************************************************
 *  Make recursive dirs
 *  index va apuntando los segmentos del path en temp
 ***************************************************************************/
PUBLIC int mkrdir(const char *path, int index, int permission)
{
    char bf[NAME_MAX];
    if(*path == '/')
        index++;
    char *p = strchr(path + index, '/');
    int len;
    if(p) {
        len = MIN(p-path, sizeof(bf)-1);
        strncpy(bf, path, len);
        bf[len]=0;
    } else {
        len = MIN(strlen(path)+1, sizeof(bf)-1);
        strncpy(bf, path, len);
        bf[len]=0;
    }

    if(access(bf, 0)!=0) {
        if(newdir(bf, permission)<0) {
            if(errno != EEXIST) {
                print_error(
                    PEF_CONTINUE,
                    "ERROR YUNETA",
                    "newdir() FAILED, %d %s, path %s",
                    errno,
                    strerror(errno),
                    bf
                );
                return -1;
            }
        }
    }
    if(p) {
        // Have you got permissions to read next directory?
        return mkrdir(path, p-path+1, permission);
    }
    return 0;
}

/****************************************************************************
 *  Recursively remove a directory
 ****************************************************************************/
PUBLIC int rmrdir(const char *root_dir)
{
#ifdef WIN32
    return -1; //TODO
#else
    struct dirent *dent;
    DIR *dir;
    struct stat st;

    if (!(dir = opendir(root_dir))) {
        return -1;
    }

    while ((dent = readdir(dir))) {
        char *dname = dent->d_name;
        if (!strcmp(dname, ".") || !strcmp(dname, ".."))
            continue;
        char *path = malloc(strlen(root_dir) + strlen(dname) + 2);
        if(!path) {
            print_error(
                PEF_CONTINUE,
                "ERROR YUNETA",
                "malloc() FAILED, %d %s",
                errno,
                strerror(errno)
            );
            closedir(dir);
            return -1;
        }
        strcpy(path, root_dir);
        strcat(path, "/");
        strcat(path, dname);

        if(stat(path, &st) == -1) {
            closedir(dir);
            free(path);
            return -1;
        }

        if(S_ISDIR(st.st_mode)) {
            /* recursively follow dirs */
            if(rmrdir(path)<0) {
                closedir(dir);
                free(path);
                return -1;
            }
        } else {
            if(unlink(path) < 0) {
                closedir(dir);
                free(path);
                return -1;
            }
        }
        free(path);
    }
    closedir(dir);
    if(rmdir(root_dir) < 0) {
        return -1;
    }
    return 0;
#endif
}

/****************************************************************************
 *  Recursively remove the content of a directory
 ****************************************************************************/
PUBLIC int rmrcontentdir(const char *root_dir)
{
#ifdef WIN32
    return -1; //TODO
#else
    struct dirent *dent;
    DIR *dir;
    struct stat st;

    if (!(dir = opendir(root_dir))) {
        return -1;
    }

    while ((dent = readdir(dir))) {
        char *dname = dent->d_name;
        if (!strcmp(dname, ".") || !strcmp(dname, ".."))
            continue;
        char *path = malloc(strlen(root_dir) + strlen(dname) + 2);
        if(!path) {
            closedir(dir);
            return -1;
        }
        strcpy(path, root_dir);
        strcat(path, "/");
        strcat(path, dname);

        if(stat(path, &st) == -1) {
            closedir(dir);
            free(path);
            return -1;
        }

        if(S_ISDIR(st.st_mode)) {
            /* recursively follow dirs */
            if(rmrdir(path)<0) {
                closedir(dir);
                free(path);
                return -1;
            }
        } else {
            if(unlink(path) < 0) {
                closedir(dir);
                free(path);
                return -1;
            }
        }
        free(path);
    }
    closedir(dir);
    return 0;
#endif
}

/***************************************************************************
 *  Copy file in kernel mode.
 *  http://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c
 ***************************************************************************/
PUBLIC int copyfile(
    const char* source,
    const char* destination,
    int permission,
    BOOL overwrite)
{
    int input, output;
    if ((input = open(source, O_RDONLY)) == -1) {
        return -1;
    }
    if ((output = newfile(destination, permission, overwrite)) == -1) {
        // error already logged
        close(input);
        return -1;
    }

    //Here we use kernel-space copying for performance reasons
#if defined(__APPLE__) || defined(__FreeBSD__)
    //fcopyfile works on FreeBSD and OS X 10.5+
    int result = fcopyfile(input, output, 0, COPYFILE_ALL);
#elif defined(__linux__)
    //sendfile will work with non-socket output (i.e. regular file) on Linux 2.6.33+
    off_t bytesCopied = 0;
    struct stat fileinfo = {0};
    fstat(input, &fileinfo);
    int result = sendfile(output, input, &bytesCopied, fileinfo.st_size);
#else
    size_t nread;
    int result = 0;
    int error = 0;
    char buf[4096];

    while (nread = read(input, buf, sizeof buf), nread > 0 && !error) {
        char *out_ptr = buf;
        size_t nwritten;

        do {
            nwritten = write(output, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                error = 1;
                result = -1;
                break;
            }
        } while (nread > 0);
    }

#endif

    close(input);
    close(output);

    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *get_last_segment(char *path)
{
    char *p = strrchr(path, '/');
    if(!p) {
        return path;
    }
    return p+1;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *pop_last_segment(char *path) // WARNING path modified
{
    char *p = strrchr(path, '/');
    if(!p) {
        return path;
    }
    *p = 0;
    return p+1;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *build_path2(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2
)
{
    snprintf(path, pathsize, "%s", dir1);
    delete_right_char(path, '/');

    if(dir2 && strlen(dir2)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir2);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    return path;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *build_path3(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2,
    const char *dir3
)
{
    snprintf(path, pathsize, "%s", dir1);
    delete_right_char(path, '/');

    if(dir2 && strlen(dir2)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir2);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir3 && strlen(dir3)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir3);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    return path;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *build_path4(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2,
    const char *dir3,
    const char *dir4
)
{
    snprintf(path, pathsize, "%s", dir1);
    delete_right_char(path, '/');

    if(dir2 && strlen(dir2)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir2);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir3 && strlen(dir3)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir3);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir4 && strlen(dir4)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir4);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    return path;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *build_path5(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2,
    const char *dir3,
    const char *dir4,
    const char *dir5
)
{
    snprintf(path, pathsize, "%s", dir1);
    delete_right_char(path, '/');

    if(dir2 && strlen(dir2)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir2);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir3 && strlen(dir3)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir3);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir4 && strlen(dir4)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir4);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir5 && strlen(dir5)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir5);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    return path;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *build_path6(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2,
    const char *dir3,
    const char *dir4,
    const char *dir5,
    const char *dir6
)
{
    snprintf(path, pathsize, "%s", dir1);
    delete_right_char(path, '/');

    if(dir2 && strlen(dir2)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir2);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir3 && strlen(dir3)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir3);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir4 && strlen(dir4)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir4);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir5 && strlen(dir5)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir5);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    if(dir6 && strlen(dir6)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir6);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    return path;
}
