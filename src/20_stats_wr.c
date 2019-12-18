/***********************************************************************
 *          STATS_WR.C
 *
 *          Simple statistics handler, writer
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "20_stats_wr.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
/***************************************************************************
 *  Get filename by time mask, using UTC or local time
 ***************************************************************************/
PRIVATE char *render_mask(
    char *bf,
    int bfsize,
    const char *mask,
    BOOL utc,
    uint64_t __t__
)
{
    struct tm *tm;
    if(utc) {
        tm = gmtime((time_t *)&__t__);
    } else {
        tm = localtime((time_t *)&__t__);
    }

    strftime(bf, bfsize, mask, tm);
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *filtra_file_mask(const char *prefix, const char *mask)
{
    static char file_mask[NAME_MAX];

    snprintf(file_mask, sizeof(file_mask),
        "%s-%s.dat",
        prefix, mask
    );
    return file_mask;
}

/***************************************************************************
   Open simple stats
 ***************************************************************************/
PUBLIC json_t *wstats_open(
    json_t *jn_config  // owned
)
{
    const char *path = kw_get_str(jn_config, "path", "", 0);
    if(empty_string(path)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "stats path EMPTY",
            NULL
        );
        JSON_DECREF(jn_config);
        return 0;
    }

    const char *group = kw_get_str(jn_config, "group", "", KW_CREATE);
    json_int_t xpermission = kw_get_int(jn_config, "xpermission", 02770, KW_CREATE);
    json_int_t rpermission = kw_get_int(jn_config, "rpermission", 0660, KW_CREATE);
    json_int_t on_critical_error = kw_get_int(jn_config, "on_critical_error", 0, KW_CREATE);
    json_t *jn_metrics = kw_get_dict(jn_config, "metrics", 0, 0);
    if(json_object_size(jn_metrics)==0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "No metrics",
            NULL
        );
        JSON_DECREF(jn_config);
        return 0;
    }

    /*-------------------------------*
     *      Check directory
     *-------------------------------*/
    char directory[PATH_MAX];
    if(!empty_string(group)) {
        snprintf(
            directory,
            sizeof(directory),
            "%s%s%s",
            path,
            (path[strlen(path)-1]=='/')?"":"/",
            group
        );
    } else {
        snprintf(
            directory,
            sizeof(directory),
            "%s",
            path
        );
    }
    if(directory[strlen(directory)-1]=='/') {
        directory[strlen(directory)-1] = 0;
    }

    if(!is_directory(directory)) {
        /*-------------------------------*
         *  Create directory
         *-------------------------------*/
        if(mkrdir(directory, 0, xpermission)<0) {
            log_critical(on_critical_error,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "path",         "%s", directory,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create directory",
                "errno",        "%s", strerror(errno),
                NULL
            );
            JSON_DECREF(jn_config);
            return 0;
        }
    }

    /*-------------------------------*
     *  Open lock file
     *-------------------------------*/
    char lockfilename[PATH_MAX];
    snprintf(
        lockfilename,
        sizeof(lockfilename),
        "%s/%s",
        directory,
        "__simple_stats__.json"
    );
    int fpx = open_exclusive(lockfilename, O_CREAT|O_WRONLY, rpermission);
    if(fpx < 0) {
        log_critical(on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "path",         "%s", directory,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot create lockfilename",
            "lockfilename", "%s", lockfilename,
            "errno",        "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn_config);
        return 0;
    }

    /*-------------------------------*
     *  Create stats
     *-------------------------------*/
    const char *keys[] = {
        "path",
        "group",
        "xpermission",
        "rpermission",
        "on_critical_error",
        "metrics",
        0
    };
    json_t *jn_dup_config = kw_clone_by_path(
        jn_config, // owned
        keys
    );
    if(json_dumpfd(jn_dup_config, fpx, JSON_INDENT(4))<0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "path",         "%s", directory,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "json_dumpfd() FAILED",
            "errno",        "%s", strerror(errno),
            NULL
        );
    }

    json_t *stats = json_object();
    json_object_set_new(stats, "directory", json_string(directory));
    json_object_set_new(
        stats,
        "config",
        jn_dup_config  // owned
    );

    kw_get_dict(stats, "file_opened_files", json_object(), KW_CREATE);
    kw_get_dict(stats, "fd_opened_files", json_object(), KW_CREATE);

    json_object_set_new(
        kw_get_dict(stats, "fd_opened_files", 0, KW_REQUIRED),
        lockfilename,
        json_integer((json_int_t)fpx)
    );

    /*-------------------------------*
     *  Crea las métricas
     *-------------------------------*/
    json_t *metrics = kw_get_dict(stats, "metrics", json_object(), KW_CREATE);

    const char *name;
    json_t *jn_metric;
    json_object_foreach(jn_metrics, name, jn_metric) {
        if(empty_string(name)) {
            continue;
        }
        /*
         *  Deja aquí la creación de subdirs para permitir nuevas métricas
         */
        char subdir[PATH_MAX];
        snprintf(
            subdir,
            sizeof(subdir),
            "%s/%s",
            directory,
            name
        );
        if(!is_directory(subdir)) {
            if(mkrdir(subdir, 0, xpermission)<0) {
                log_critical(on_critical_error,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "process",      "%s", get_process_name(),
                    "hostname",     "%s", get_host_name(),
                    "pid",          "%d", get_pid(),
                    "path",         "%s", subdir,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "Cannot create directory",
                    "errno",        "%s", strerror(errno),
                    NULL
                );
                json_decref(stats);
                return 0;
            }
        }

        json_t *metric = kw_get_dict(metrics, name, json_object(), KW_CREATE);
        kw_get_str(metric, "directory", subdir, KW_CREATE);
        kw_get_int(metric, "last_t", 0, KW_CREATE);

        json_t *masks = kw_get_list(metric, "masks", json_array(), KW_CREATE);
        int idx;
        json_t *jn_mask;
        json_array_foreach(jn_metric, idx, jn_mask) {
            json_t *mask = json_object();
            const char *id = kw_get_str(mask, "id",
                kw_get_str(jn_mask, "id", 0, KW_REQUIRED),
                KW_CREATE
            );
            kw_get_str(mask, "metric_type",
                kw_get_str(jn_mask, "metric_type", "average", KW_REQUIRED),
                KW_CREATE
            );
            kw_get_dict_value(mask, "value_type",
                kw_get_dict_value(jn_mask, "value_type", json_integer(0), KW_REQUIRED),
                KW_CREATE
            );
            kw_get_str(mask, "filename_mask",
                filtra_file_mask(id,
                    filtra_time_mask(kw_get_str(jn_mask, "filename_mask", "MON", KW_REQUIRED))
                ),
                KW_CREATE
            );
            kw_get_str(mask, "time_mask",
                filtra_time_mask(kw_get_str(jn_mask, "time_mask", "MIN", KW_REQUIRED)),
                KW_CREATE
            );

            kw_get_str(mask, "filename", "", KW_CREATE);
            kw_get_str(mask, "stime", "", KW_CREATE);

            json_t *value_type = kw_get_dict_value(mask, "value_type", 0, KW_REQUIRED);
            if(json_is_real(value_type)) {
                kw_get_real(mask, "value", 0, KW_CREATE);
            } else {
                kw_get_int(mask, "value", 0, KW_CREATE);
            }
            json_array_append_new(masks, mask);
        }
    }

    //log_debug_json(0, stats, "__simple_stats__.json");
    return stats;
}

/***************************************************************************
   Close simple stats
 ***************************************************************************/
PUBLIC void wstats_close(
    json_t *stats
)
{
    /*-------------------------------*
     *  Close FILE files
     *-------------------------------*/
    const char *key;
    json_t *jn_value;
    void *temp;
    json_t *file_opened_files = kw_get_dict(stats, "file_opened_files", 0, KW_REQUIRED);
    json_object_foreach_safe(file_opened_files, temp, key, jn_value) {
        FILE *file = (FILE *)(size_t)kw_get_int(file_opened_files, key, 0, KW_REQUIRED);
        if(file) {
            fclose(file);
        }
        json_object_del(file_opened_files, key);
    }

    /*-------------------------------*
     *  Close fd files
     *-------------------------------*/
    json_t *fd_opened_files = kw_get_dict(stats, "fd_opened_files", 0, KW_REQUIRED);
    json_object_foreach_safe(fd_opened_files, temp, key, jn_value) {
        int fd = kw_get_int(fd_opened_files, key, 0, KW_REQUIRED);
        if(fd >= 0) {
            close(fd);
        }
        json_object_del(fd_opened_files, key);
    }

    /*-------------------------------*
     *  Free stats
     *-------------------------------*/
    JSON_DECREF(stats);
}

/***************************************************************************
   Close file
 ***************************************************************************/
PRIVATE void close_file(json_t *stats, json_t *metric, const char *filename)
{
    const char *directory = json_string_value(json_object_get(metric, "directory"));

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", directory, filename);

    if(kw_has_subkey(stats, "file_opened_files", path)) {
        FILE *file = (FILE *)(size_t)kw_get_int(
            kw_get_dict(stats, "file_opened_files", 0, KW_REQUIRED),
            path,
            0,
            0
        );
        if(file) {
            fclose(file);
        }
        kw_delete_subkey(stats, "file_opened_files", path);
    }
}

/***************************************************************************
   Delete file
 ***************************************************************************/
PRIVATE void delete_file(json_t *stats, json_t *metric, const char *filename)
{
    const char *directory = json_string_value(json_object_get(metric, "directory"));

    close_file(stats, metric, filename);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", directory, filename);

    if(is_regular_file(path)) {
        if(unlink(path)<0) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot delete file",
                "errno",        "%s", strerror(errno),
                NULL
            );
        }
    }
}

/***************************************************************************
   Save metric value to file
 ***************************************************************************/
PRIVATE void save_metric_value_to_file(
    json_t *stats,
    json_t *metric,
    const char *filename,
    uint64_t t,
    json_t *value
)
{
    char path[PATH_MAX];

    const char *directory = json_string_value(json_object_get(metric, "directory"));
    snprintf(path, sizeof(path), "%s/%s", directory, filename);

    FILE *file = (FILE *)(size_t)kw_get_int(
        kw_get_dict(stats, "file_opened_files", 0, KW_REQUIRED),
        path,
        0,
        0
    );
    if(!file) {
        file = fopen(path, "a+");
        if(!file) {
            log_error(kw_get_int(stats, "config`on_critical_error", 0, KW_REQUIRED),
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create file",
                "path",         "%s", path,
                "errno",        "%s", strerror(errno),
                NULL
            );
            return;
        }

        json_object_set_new(
            kw_get_dict(stats, "file_opened_files", 0, KW_REQUIRED),
            path,
            json_integer((json_int_t)(size_t)file)
        );
    }

    stats_record_t stats_record;
    stats_record.t = t;
    if(json_is_real(value)) {
        //fprintf(file, "[%"PRIu64", %f]", t, json_real_value(value));
        stats_record.v.d = json_real_value(value);
    } else {
        //fprintf(file, "[%"PRIu64", %"JSON_INTEGER_FORMAT"]", t, json_integer_value(value));
        stats_record.v.i64 = json_integer_value(value);
    }
    fwrite(&stats_record, sizeof(stats_record_t), 1, file);

    if(0) { // TODO TEST
        struct tm *tm;
        char d[80];
        tm = gmtime((time_t *)&t);
        tm2timestamp(d, sizeof(d), tm);

        trace_msg("%"PRIu64", %s ==> %s", t, d, filename);
    }
}

/***************************************************************************
   Calculate new value based in metric type
 ***************************************************************************/
PRIVATE json_t *calculate_value(
    const char *metric_type,
    BOOL new_range,
    time_t last_t,   // UTC time
    time_t t,       // UTC time
    json_t *last_value,
    json_t *jn_value
)
{
    json_t *new_value = 0;

    if(json_is_real(last_value)) {
        double last_value_ = json_real_value(last_value);
        double value = jn2real(jn_value);

        if(strcasecmp(metric_type, "average")==0) {
            if(new_range) {
                //value = value;
            } else {
                value = (value + last_value_)/2;
            }

        } else if(strcasecmp(metric_type, "min")==0) {
            if(new_range) {
                //value = value;
            } else {
                if(value > last_value_) {
                    value = last_value_;
                }
            }

        } else if(strcasecmp(metric_type, "max")==0) {
            if(new_range) {
                //value = value;
            } else {
                if(value < last_value_) {
                    value = last_value_;
                }
            }

        } else if(strcasecmp(metric_type, "sum")==0) {
            if(new_range) {
                //value = value;
            } else {
                value += last_value_;
            }
        }

        new_value = json_real(value);

    } else {
        json_int_t last_value_ = json_integer_value(last_value);
        json_int_t value = jn2integer(jn_value);

        if(strcasecmp(metric_type, "average")==0) {
            if(new_range) {
                //value = value;
            } else {
                value = (value + last_value_)/2;
            }

        } else if(strcasecmp(metric_type, "min")==0) {
            if(new_range) {
                //value = value;
            } else {
                if(value > last_value_) {
                    value = last_value_;
                }
            }

        } else if(strcasecmp(metric_type, "max")==0) {
            if(new_range) {
                //value = value;
            } else {
                if(value < last_value_) {
                    value = last_value_;
                }
            }

        } else if(strcasecmp(metric_type, "sum")==0) {
            if(new_range) {
                //value = value;
            } else {
                value += last_value_;
            }
        }

        new_value = json_integer(value);
    }

    return new_value;
}

/***************************************************************************
   Add stats
 ***************************************************************************/
PUBLIC void wstats_add_value(
    json_t *stats,
    const char *metric_name,
    time_t t,   // UTC time
    json_t *jn_value  // owned
)
{
    if(!t) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Time cannot be zero!",
            "directory",    "%s", kw_get_str(stats, "directory", "", KW_REQUIRED),
            NULL
        );
        JSON_DECREF(jn_value);
        return;
    }

    json_t *metric = json_object_get(
        json_object_get(
            stats,
            "metrics"
        ),
        metric_name
    );
    if(!metric) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Metric not configured",
            "directory",    "%s", kw_get_str(stats, "directory", "", KW_REQUIRED),
            "metric",       "%s", metric_name,
            NULL
        );
        JSON_DECREF(jn_value);
        return;
    }

    uint64_t last_t = json_integer_value(json_object_get(metric, "last_t"));
    if(t < last_t) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Time lower than last t",
            "directory",    "%s", kw_get_str(stats, "directory", "", KW_REQUIRED),
            "last_t",       "%ld", (unsigned long)last_t,
            "t",            "%ld", (unsigned long)t,
            NULL
        );
        JSON_DECREF(jn_value);
        return;
    }

    json_object_set_new(metric, "last_t", json_integer(t));

    json_t *masks = json_object_get(metric, "masks");
    char filename_[NAME_MAX];
    char stime_[NAME_MAX];
    int idx;
    json_t *mask;
    json_array_foreach(masks, idx, mask) {
        const char *metric_type = json_string_value(json_object_get(mask, "metric_type"));
        const char *filename_mask = json_string_value(json_object_get(mask, "filename_mask"));
        const char *time_mask = json_string_value(json_object_get(mask, "time_mask"));

        const char *filename = json_string_value(json_object_get(mask, "filename"));
        const char *stime = json_string_value(json_object_get(mask, "stime"));

        render_mask(filename_, sizeof(filename_), filename_mask, TRUE, t);
        render_mask(stime_, sizeof(stime_), time_mask, TRUE, t);

        if(last_t == 0) {
            filename = filename_;
            stime = stime_;
            json_object_set_new(mask, "filename", json_string(filename_));
            json_object_set_new(mask, "stime", json_string(stime_));
        }

        /*
         *  Changed filename?
         */
        if(strcmp(filename, filename_)!=0) {
            // Close old file
            close_file(
                stats,
                metric,
                filename
            );

            // New file, delete if exists
            delete_file(
                stats,
                metric,
                filename_
            );
            json_object_set_new(mask, "filename", json_string(filename_));
        }

        /*
         *  Changed stime?
         */
        BOOL new_range = FALSE;
        if(strcmp(stime, stime_)!=0) {
            // New range
            if(last_t > 0) {
                // Save current acumulated time
                save_metric_value_to_file(
                    stats,
                    metric,
                    filename,
                    last_t,
                    json_object_get(mask, "value")
                );
                json_object_set_new(mask, "stime", json_string(stime_));
            }
            new_range = TRUE;
        }

        /*
         *  Calculate new value
         */
        json_t *new_value = calculate_value(
            metric_type,
            new_range,
            last_t,
            t,
            json_object_get(mask, "value"),
            jn_value
        );
        json_object_set_new(mask, "value", new_value);
    }

    JSON_DECREF(jn_value);
}

/***************************************************************************
   Save partial data
 ***************************************************************************/
PUBLIC int wstats_save(
    json_t *stats
)
{
    static const char *keys[] = {
        "last_t",
        "filename",
        "stime",
        "value",
        0
    };
    json_t *jn_data = kw_get_propagated_key_values(stats, "id", keys);

    char path[PATH_MAX];
    const char *directory = json_string_value(json_object_get(stats, "directory"));
    snprintf(path, sizeof(path), "%s/%s", directory, "__partial_data__.json");

    int ret = json_dump_file(jn_data, path, JSON_INDENT(4));
    json_decref(jn_data);

    const char *key;
    json_t *jn_value;
    json_t *file_opened_files = kw_get_dict(stats, "file_opened_files", 0, KW_REQUIRED);
    json_object_foreach(file_opened_files, key, jn_value) {
        FILE *file = (FILE *)(size_t)kw_get_int(file_opened_files, key, 0, KW_REQUIRED);
        if(file) {
            fflush(file);
        }
    }

    return ret;
}

/***************************************************************************
   Restore partial data
 ***************************************************************************/
PUBLIC int wstats_restore(
    json_t *stats
)
{
    char path[PATH_MAX];
    const char *directory = json_string_value(json_object_get(stats, "directory"));
    snprintf(path, sizeof(path), "%s/%s", directory, "__partial_data__.json");

    size_t flags = 0;
    json_error_t error;
    json_t *jn_data = json_load_file(path, flags, &error);
    if(!jn_data) {
        return -1;
    }
    return kw_put_propagated_key_values(stats, "id", jn_data);
}


