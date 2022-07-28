/****************************************************************************
 *              ROTATORY.C
 *              Log by week's days or or month's days or year's days
 *              Copyright (c) 1996-2014 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE  /* get gnu version of basename() */
#endif

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#ifdef WIN32
    #include  <io.h>
    #include <sys/stat.h>
    #define fopen64 fopen
    #define access _access
    #define close _close
    #define unlink _unlink
    #define fileno _fileno
    #define fstatvfs64 _fstat
    #define statvfs64 _stat
#else
    #include <libgen.h>
    #include <dirent.h>
    #include <sys/statvfs.h>
    #include <unistd.h>
#endif
#include "03_rotatory.h"

/*****************************************************************
 *          Constants
 *****************************************************************/
#define MAX_COUNTER_STATVFS     100
#define DEFAULT_BUFFER_SIZE     (64*1024)

/*****************************************************************
 *          Structures
 *****************************************************************/
typedef struct rotatory_log_s {
    DL_ITEM_FIELDS
    char schema[32], path[2*NAME_MAX+1], port[32];
    size_t buffer_size;
    uint64_t max_megas_rotatoryfile_size;
    size_t min_free_disk_percentage;
    int xpermission;            // permission for directories and executable files.
    int rpermission;            // permission for regular files.
    pe_flag_t pe_flag;          // Exit if cannot create rotatory file

    uint16_t counter_statvfs;
    char log_directory[NAME_MAX];   // from path
    char filenamemask[NAME_MAX];    // from path
    char filename[NAME_MAX];        // current filename
    int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename);
    void *user_data;
    FILE *flog;
    int week_day;
    int month_day;
    int year_day;
    char *buffer;
    char temp[8*1024];

} rotatory_log_t;

/*****************************************************************
 *          Data
 *****************************************************************/
PRIVATE char __initialized__ = 0;
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE dl_list_t dl_clients;
PRIVATE const char *priority_names[]={
    "EMERG",
    "ALERT",
    "CRITICAL",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
    "STATS",
    "MONITOR",
    "AUDIT",
    0
};


/*****************************************************************
 *          Prototypes
 *****************************************************************/
PRIVATE void _rotatory_trunk(rotatory_log_t *rotatory_log);
PRIVATE void _rotatory_flush(rotatory_log_t* hr);
PRIVATE BOOL _get_rotatory_filename(rotatory_log_t *rotatory_log);
PRIVATE int _rotatory(rotatory_log_t *hr, const char *bf, size_t len);
PRIVATE int _translate_mask(rotatory_log_t *hr);


/*****************************************************************
 *
 *****************************************************************/
PUBLIC int rotatory_start_up(void)
{
    if(__initialized__) {
        return -1;
    }
    if (!atexit_registered) {
        atexit(rotatory_end);
        atexit_registered = 1;
    }

    dl_init(&dl_clients);
    __initialized__ = TRUE;
    return 0;
}

/*****************************************************************
 *  Close all
 *****************************************************************/
PUBLIC void rotatory_end(void)
{
    rotatory_log_t *hr;

    while((hr=dl_first(&dl_clients))) {
        rotatory_close(hr);
    }
    __initialized__ = FALSE;
}

/*****************************************************************
 *  Return NULL on error
 *****************************************************************/
PUBLIC hrotatory_t rotatory_open(
    const char* path,
    size_t bf_size,
    size_t max_megas_rotatoryfile_size,
    size_t min_free_disk_percentage,
    int xpermission,
    int rpermission,
    BOOL exit_on_fail)
{
    rotatory_log_t *hr = 0;

    if(!__initialized__) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "rotatory_open(): rotatory not initialized"
        );
        return 0;
    }

    /*-------------------------------------*
     *          Check parameters
     *-------------------------------------*/
    if(!path || !*path) {
        print_error(
            PEF_SILENCE,
            "ERROR YUNETA",
            "path EMPTY"
        );
        return 0;
    }

    if(bf_size <=0) {
        bf_size = DEFAULT_BUFFER_SIZE;
    }
    if(max_megas_rotatoryfile_size <= 0) {
        max_megas_rotatoryfile_size = 8;
    }
    if(min_free_disk_percentage <= 0) {
        min_free_disk_percentage = 10;
    }
    if(!xpermission) {
        xpermission = 02775;
    }
    if(!rpermission) {
        rpermission = 0664;
    }

    /*-------------------------------------*
     *          Alloc memory
     *-------------------------------------*/
    hr = calloc(1, sizeof(rotatory_log_t));
    if(!hr) {
        print_error(
            PEF_ABORT,
            "ERROR YUNETA",
            "rotatory_open(): No MEMORY for %d",
            sizeof(rotatory_log_t)
        );
        return 0;
    }
    strncpy(hr->path, path, sizeof(hr->path) -1);
    hr->buffer_size = bf_size;
    hr->max_megas_rotatoryfile_size = max_megas_rotatoryfile_size;
    hr->min_free_disk_percentage = min_free_disk_percentage;
    hr->xpermission = xpermission;
    hr->rpermission = rpermission;
    hr->pe_flag = exit_on_fail?PEF_EXIT:PEF_CONTINUE;

    hr->buffer = calloc(1, hr->buffer_size);
    if(!hr->buffer) {
        print_error(
            PEF_ABORT,
            "ERROR YUNETA",
            "rotatory_open(): No MEMORY for %d",
            bf_size
        );
        free(hr);
        return 0;
    }

    /*------------------------------------------------*
     *  Split path in log_directory and filenamemask
     *------------------------------------------------*/
    char *filenamemask = basename(hr->path);
    strncpy(hr->filenamemask, filenamemask, sizeof(hr->filenamemask) -1);
    char *log_directory = dirname(hr->path);
    strncpy(hr->log_directory, log_directory, sizeof(hr->log_directory) -1);

    /*-----------------------------*
     *  Create the log directory
     *-----------------------------*/
    if(access(hr->log_directory, 0)!=0) {
        if(mkrdir(hr->log_directory, 0, hr->xpermission)<0) {
            print_error(
                hr->pe_flag,
                "YUNETA ERROR",
                "rotatory_open(): Cannot create '%s' directory, %s",
                hr->log_directory,
                strerror(errno)
            );
            rotatory_close(hr);
            return 0;
        }
    }
    /*-----------------------------*
     *  Make initial filename
     *-----------------------------*/
    _translate_mask(hr);
    snprintf(hr->path, sizeof(hr->path), "%s/%s", hr->log_directory, hr->filename);
    if(access(hr->path, 0)!=0) {
        int fd = newfile(hr->path, hr->rpermission, FALSE);
        if(fd < 0) {
            print_error(
                hr->pe_flag,
                "YUNETA ERROR",
                "rotatory_open(): Cannot create '%s' file, %s",
                hr->path,
                strerror(errno)
            );
            rotatory_close(hr);
            return 0;
        }
        close(fd);
    }
    hr->flog = fopen64(hr->path, "a");
    if(!hr->flog) {
        print_error(
            hr->pe_flag,
            "YUNETA ERROR",
            "rotatory_open(): Cannot open '%s' file, %s",
            hr->path,
            strerror(errno)
        );
        rotatory_close(hr);
        return 0;
    }

    /*-------------------------------------*
     *  Add to list and create rotatory
     *-------------------------------------*/
    dl_add(&dl_clients, hr);

    return hr;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC void rotatory_close(hrotatory_t hr_)
{
    rotatory_log_t *hr = hr_;

    if(!__initialized__ || !hr) {
        return;
    }

    if(hr->flog) {
        _rotatory_flush(hr);
        fclose(hr->flog);
        hr->flog = 0;
    }
    if(dl_find(&dl_clients, hr)) {
        dl_delete(&dl_clients, hr, 0);
    }
    free(hr->buffer);
    free(hr);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int rotatory_subscribe2newfile(
    hrotatory_t hr_,
    int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename),
    void *user_data)
{
    rotatory_log_t *hr = hr_;
    hr->cb_newfile = cb_newfile;
    hr->user_data = user_data;

    return 0;
}

/*****************************************************************
 *   Return bytes written
 *****************************************************************/
PUBLIC int rotatory_write(hrotatory_t hr_, int priority, const char* bf, size_t len)
{
    rotatory_log_t *hr = hr_;

    if(!hr || !bf) {
        return -1;
    }
    if(priority < 0 || priority > LOG_AUDIT) {
        priority = LOG_DEBUG;
    }

    if(priority == LOG_AUDIT) {
        // without header
        _rotatory(hr, bf, len);
    } else {
        char spriority[64];
        snprintf(
            spriority,
            sizeof(spriority),
            "%s: ",
            priority_names[priority]
        );
        _rotatory(hr, spriority, strlen(spriority));
        _rotatory(hr, bf, len);
    }
    _rotatory(hr, "\n", 1);
    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int rotatory_fwrite(hrotatory_t hr_, int priority, const char *format, ...)
{
    rotatory_log_t *hr = hr_;
    va_list ap;

    if(!hr) {
        // silence
        return -1;
    }
    va_start(ap, format);
    vsnprintf(
        hr->buffer,
        hr->buffer_size,
        format,
        ap
    );
    va_end(ap);

    return rotatory_write(hr, priority, hr->buffer, strlen(hr->buffer));
}

/*****************************************************************
 *  if hr is null trunk all files
 *****************************************************************/
PUBLIC void rotatory_trunk(hrotatory_t hr)
{
    if(hr) {
        _rotatory_trunk(hr);
        return;
    }

    hr = dl_first(&dl_clients);
    while(hr) {
        _rotatory_trunk(hr);
        hr = dl_next(hr);
    }
}

/*****************************************************************
 *  if hr is null flush all files
 *****************************************************************/
PUBLIC void rotatory_flush(hrotatory_t hr)
{
    if(hr) {
        _rotatory_flush(hr);
        return;
    }

    hr = dl_first(&dl_clients);
    while(hr) {
        _rotatory_flush(hr);
        hr = dl_next(hr);
    }
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE void _rotatory_trunk(rotatory_log_t *hr)
{
    if(hr->flog) {
        rotatory_flush(hr);
        fclose(hr->flog);
        hr->flog = fopen64(hr->path, "w");
        if(!hr->flog) {
            print_error(
                hr->pe_flag,
                "YUNETA ERROR",
                "_rotatory_trunk(): Cannot open '%s' file, %s",
                hr->path,
                strerror(errno)
            );
        }
    }
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE void _rotatory_flush(rotatory_log_t *hr)
{
    if(hr->flog) {
#ifdef USE_LOCK_FILE
        lock_file(fileno(hr->flog));
#endif
        fflush(hr->flog);
#ifdef USE_LOCK_FILE
        unlock_file(fileno(hr->flog));
#endif
    }
}

/*****************************************************************
 *  Return TRUE is filename has changed
 *****************************************************************/
PRIVATE BOOL _get_rotatory_filename(rotatory_log_t *hr)
{
    BOOL change_file = 0;
    char path[2*NAME_MAX+1];
    _translate_mask(hr);

    snprintf(path, sizeof(path), "%s/%s", hr->log_directory, hr->filename);

    if(strcmp(path, hr->path)!=0) {
        change_file = 1;
    } else {
        if(access(hr->path, 0)!=0) {
            change_file = 1;
        }
    }

    return change_file;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE int _rotatory(rotatory_log_t *hr, const char *bf, size_t len)
{
    static char disk_full_informed=0;

    if(disk_full_informed) {
        return -1;
    }
    BOOL change_file = _get_rotatory_filename(hr);

    /*
     *  Check the size of file
     */
    if(hr->flog) {
        if(hr->max_megas_rotatoryfile_size) {
            uint64_t siz = filesize2(fileno(hr->flog));
            siz /= 1*1024*1024;
            if(siz > hr->max_megas_rotatoryfile_size) {
                /*
                 *  Rename to OLD and create a new file
                 */
                rotatory_flush(hr);
                fclose(hr->flog);
                hr->flog = 0;

                char filename_old[2*NAME_MAX+5];
                snprintf(filename_old, sizeof(filename_old), "%s.OLD",
                    hr->path
                );
                if(access(filename_old, 0)==0) {
                    if(unlink(filename_old)<0) {
                        print_error(
                            PEF_CONTINUE,
                            "YUNETA ERROR",
                            "_rotatory(): Cannot remove '%s', %s",
                            filename_old,
                            strerror(errno)
                        );
                    }
                }
                if(rename(hr->path, filename_old) < 0) {
                    print_error(
                        PEF_CONTINUE,
                        "YUNETA ERROR",
                        "_rotatory(): Cannot rename '%s' to '%s', %s",
                        hr->path,
                        filename_old,
                        strerror(errno)
                    );
                }
                change_file = 1;
            }
        }
    }

    if(change_file) {
        if(hr->flog) {
            rotatory_flush(hr);
            fclose(hr->flog);
            hr->flog = 0;
        }
        if(access(hr->log_directory, 0)!=0) {
            // Creat the directory
            if(mkrdir(hr->log_directory, 0, hr->xpermission)<0) {
                print_error(
                    hr->pe_flag,
                    "YUNETA ERROR",
                    "_rotatory(): Cannot create '%s' directory, %s",
                    hr->log_directory,
                    strerror(errno)
                );
                return -1;
            }
        }

        char lastpath[NAME_MAX];
        strncpy(lastpath, hr->path, sizeof(lastpath)-1);
        snprintf(hr->path, sizeof(hr->path), "%s/%s", hr->log_directory, hr->filename);
        if(access(hr->path, 0)!=0) {
            int fd = newfile(hr->path, hr->rpermission, FALSE);
            if(fd < 0) {
                print_error(
                    hr->pe_flag,
                    "YUNETA ERROR",
                    "_rotatory(): Cannot create '%s' file, %s",
                    hr->path,
                    strerror(errno)
                );
                return -1;
            }
            close(fd);
        }
        hr->flog = fopen64(hr->path, "w");
        if(!hr->flog) {
            print_error(
                hr->pe_flag,
                "YUNETA ERROR",
                "_rotatory(): Cannot open '%s' file, %s",
                hr->path,
                strerror(errno)
            );
            return -1;
        }
        if(hr->cb_newfile) {
            (hr->cb_newfile)(hr->user_data, lastpath, hr->path);
        }
    }

    hr->counter_statvfs++;
    hr->counter_statvfs = hr->counter_statvfs % MAX_COUNTER_STATVFS;
    if(hr->flog && hr->counter_statvfs==0) {
        /*
         *  Check filesystem free only periodically,
         *  each MAX_COUNTER_STATVFS times.
         */
#ifdef __linux__
        struct statvfs64 fiData;
        if(fstatvfs64(fileno(hr->flog), &fiData) == 0) {
            int free_percent = (fiData.f_bavail * 100)/fiData.f_blocks;
            if(free_percent < hr->min_free_disk_percentage) {
                // No escribo nada con %free menor que x
                if(!disk_full_informed) {
                    print_error(
                        PEF_CONTINUE,
                        "YUNETA ERROR",
                        "rotatory(): stop logging because full disk: %d%% free (<%d%%)",
                        free_percent,
                        hr->min_free_disk_percentage
                    );
                    rotatory_flush(hr);
                    disk_full_informed = 1;
                }
                return -1;
            }
        }
#endif /* __linux__ */
    }

    // TODO perhaps I would remove lock file in order to get more speed.
    // block to write
    if(hr->flog) {
#ifdef USE_LOCK_FILE
        lock_file(fileno(hr->flog));
#endif
        int ret = fwrite(bf, 1, len,hr->flog);
#ifdef USE_LOCK_FILE
        unlock_file(fileno(hr->flog));
#endif
        if(ret <= 0) {
            print_error(
                PEF_CONTINUE,
                "YUNETA ERROR",
                "_rotatory(): vfprintf() FAILED, %s",
                strerror(errno)
            );
            fclose(hr->flog);
            hr->flog = 0;
            return ret;
        }
    }

    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE int _translate_mask(rotatory_log_t *hr)
{
    formatnowdate(
        hr->filename,
        sizeof(hr->filename),
        hr->filenamemask
    );

    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC const char *rotatory_path(hrotatory_t hr_)
{
    rotatory_log_t *hr = hr_;
    return hr->path;
}
