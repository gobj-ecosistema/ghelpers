/***********************************************************************
 *          STATS_WR.C
 *
 *          Simple statistics handler, writer
 *
 *          Copyright (c) 2018,2019 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
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
PRIVATE int wstats_restore(
    json_t *variable
);

/***************************************************************
 *              Data
 ***************************************************************/
static const json_desc_t stats_json_desc[] = {
// Name                 Type    Default
{"path",                "str",  ""},
{"xpermission" ,        "int",  "02770"}, // Use in creation, default 02770;
{"rpermission",         "int",  "0660"}, // Use in creation, default 0660;
{"on_critical_error",   "int",  "0"},  // Volatil, default LOG_NONE (Zero to avoid restart)
{0}
};
/***************************************************************************
 *  Get filename by time metric, using UTC or local time
 ***************************************************************************/
PRIVATE char *render_mask(
    char *bf,
    int bfsize,
    const char *metric,
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

    strftime(bf, bfsize, metric, tm);
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *filtra_file_mask(const char *prefix, const char *metric)
{
    static char file_mask[NAME_MAX];

    snprintf(file_mask, sizeof(file_mask),
        "%s-%s.dat",
        prefix, metric
    );
    return file_mask;
}

/***************************************************************************
   Open simple stats
 ***************************************************************************/
PUBLIC json_t *wstats_open(
    json_t *jn_stats  // owned
)
{
    const char *path = kw_get_str(jn_stats, "path", "", 0);
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
        JSON_DECREF(jn_stats);
        return 0;
    }

    json_t *stats = create_json_record(stats_json_desc);
    json_object_update_existing(stats, jn_stats);
    JSON_DECREF(jn_stats);

    /*-------------------------------------*
     *  Build stats directory and
     *  __simple_stats__.json metadata file
     *-------------------------------------*/
    json_int_t on_critical_error = kw_get_int(stats, "on_critical_error", 0, 0);

    char directory[PATH_MAX];
    build_path2(
        directory,
        sizeof(directory),
        path,
        ""
    );
    kw_set_dict_value(stats, "directory", json_string(directory));

    int fd = -1;
    if(file_exists(directory, "__simple_stats__.json")) {
        json_t *jn_disk = load_persistent_json(
            directory,
            "__simple_stats__.json",
            on_critical_error,
            &fd,
            TRUE, // exclusive
            FALSE // silence
        );
        json_object_update_existing(stats, jn_disk);
        JSON_DECREF(jn_disk);
    } else {
        /*
         *  I'm MASTER
         */
        int xpermission = kw_get_int(stats, "xpermission", 02770, KW_REQUIRED);
        int rpermission = kw_get_int(stats, "rpermission", 0660, KW_REQUIRED);
        json_t *jn_stats = json_object();
        kw_get_int(jn_stats, "rpermission", rpermission, KW_CREATE);
        kw_get_int(jn_stats, "xpermission", xpermission, KW_CREATE);

        save_json_to_file(
            directory,
            "__simple_stats__.json",
            xpermission,
            rpermission,
            on_critical_error,
            TRUE,   //create
            TRUE,  //only_read
            jn_stats  // owned
        );
        // Re-open
        json_t *jn_disk = load_persistent_json(
            directory,
            "__simple_stats__.json",
            on_critical_error,
            &fd,
            TRUE, // exclusive
            FALSE // silence
        );
        json_object_update_existing(stats, jn_disk);
        JSON_DECREF(jn_disk);
    }

    /*
     *  Load Only read, volatil, defining in run-time
     */
    const char *database = get_last_segment(directory);
    kw_get_str(stats, "database", database, KW_CREATE);
    kw_get_dict(stats, "file_opened_files", json_object(), KW_CREATE);
    kw_get_dict(stats, "fd_opened_files", json_object(), KW_CREATE);
    kw_get_dict(stats, "variables", json_object(), KW_CREATE);

    kw_set_subdict_value(stats, "fd_opened_files", "__simple_stats__.json", json_integer(fd));

    return stats;
}

/***************************************************************************
   Add new variable
 ***************************************************************************/
PUBLIC json_t *wstats_add_variable(
    json_t *stats,
    json_t *jn_variable  // owned
)
{
    const char *variable_name = kw_get_str(jn_variable, "variable_name", "", KW_REQUIRED);
    const char *group = kw_get_str(jn_variable, "group", "", 0);
    json_int_t version = kw_get_int(jn_variable, "version", -1, KW_WILD_NUMBER|KW_REQUIRED);
    if(empty_string(variable_name)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "variable name EMPTY",
            NULL
        );
        JSON_DECREF(jn_variable);
        return 0;
    }

    /*
     *  Recover stats parameters
     */
    const char *directory = kw_get_str(stats, "directory", "", KW_REQUIRED);
    int xpermission = kw_get_int(stats, "xpermission", 0660, KW_REQUIRED);
    int rpermission = kw_get_int(stats, "rpermission", 0660, KW_REQUIRED);
    json_int_t on_critical_error = kw_get_int(stats, "on_critical_error", 0, 0);

    char full_variable_name[NAME_MAX];
    if(empty_string(group)) {
        snprintf(full_variable_name, sizeof(full_variable_name), "%s", variable_name);
    } else {
        snprintf(full_variable_name, sizeof(full_variable_name), "%s-%s", group, variable_name);
    }

    json_t *variables = kw_get_dict(stats, "variables", 0, KW_REQUIRED);
    json_t *variable = kw_get_dict(variables, full_variable_name, 0, 0);
    if(variable) {
        JSON_DECREF(jn_variable);
        return variable;
    }
    variable = kw_get_dict(variables, full_variable_name, json_object(), KW_CREATE);

    /*
     *  Check if __variable__.json already exists or create it
     */
    char subdir[PATH_MAX];
    build_path3(subdir, sizeof(subdir), directory, group, variable_name);

    do {
        if(file_exists(subdir, "__variable__.json")) {
            json_t *old_jn_variable = load_json_from_file(
                subdir,
                "__variable__.json",
                on_critical_error
            );
            json_int_t old_version = kw_get_int(
                old_jn_variable,
                "version",
                -1,
                KW_WILD_NUMBER
            );
            if(version <= old_version) {
                JSON_DECREF(jn_variable);
                jn_variable = old_jn_variable;
                break;
            }
            JSON_DECREF(old_jn_variable);
        }

        const char *database = kw_get_str(stats, "database", "", KW_REQUIRED);
        kw_get_str(jn_variable, "database", database, KW_CREATE);
        log_info(0,
            "gobj",             "%s", __FILE__,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "Creating Metric file.",
            "database",         "%s", database,
            "variable_name",    "%s", variable_name,
            "group",            "%s", group?group:"",
            "variable_subdir",  "%s", subdir,
            NULL
        );
        JSON_INCREF(jn_variable);
        save_json_to_file(
            subdir,
            "__variable__.json",
            xpermission,
            rpermission,
            on_critical_error,
            TRUE,       // Create file if not exists or overwrite.
            FALSE,      // only_read
            jn_variable   // owned
        );
    } while(0);

    kw_get_str(variable, "directory", subdir, KW_CREATE);
    kw_get_int(variable, "last_t", 0, KW_CREATE);

    json_t *metrics_ = kw_get_dict_value(jn_variable, "metrics", 0, KW_REQUIRED);
    json_t *metrics = kw_get_list(variable, "metrics", json_array(), KW_CREATE);
    int idx;
    json_t *jn_metric;
    json_array_foreach(metrics_, idx, jn_metric) {
        json_t *metric = json_object();
        const char *id = kw_get_str(metric, "id",
            kw_get_str(jn_metric, "id", 0, KW_REQUIRED),
            KW_CREATE
        );
        kw_get_str(metric, "metric_type",
            kw_get_str(jn_metric, "metric_type", "average", KW_REQUIRED),
            KW_CREATE
        );

        json_t *value_type = kw_get_dict_value(jn_metric, "value_type", json_integer(0), KW_REQUIRED);
        JSON_INCREF(value_type);
        kw_get_dict_value(metric, "value_type",
            value_type,
            KW_CREATE
        );
        kw_get_str(metric, "filename_mask",
            filtra_file_mask(id,
                filtra_time_mask(kw_get_str(jn_metric, "filename_mask", "MON", KW_REQUIRED))
            ),
            KW_CREATE
        );
        kw_get_str(metric, "time_mask",
            filtra_time_mask(kw_get_str(jn_metric, "time_mask", "MIN", KW_REQUIRED)),
            KW_CREATE
        );

        kw_get_str(metric, "filename", "", KW_CREATE);
        kw_get_str(metric, "stime", "", KW_CREATE);

        if(json_is_real(value_type)) {
            kw_get_real(metric, "value", 0, KW_CREATE);
        } else {
            kw_get_int(metric, "value", 0, KW_CREATE);
        }
        json_array_append_new(metrics, metric);
    }

    wstats_restore(variable);

    JSON_DECREF(jn_variable);
    return variable;
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
PRIVATE void close_file(json_t *stats, json_t *variable, const char *filename)
{
    const char *directory = json_string_value(json_object_get(variable, "directory"));

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
PRIVATE void delete_file(json_t *stats, json_t *variable, const char *filename)
{
    const char *directory = json_string_value(json_object_get(variable, "directory"));

    close_file(stats, variable, filename);

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
   Save variable value to file
 ***************************************************************************/
PRIVATE void save_variable_value_to_file(
    json_t *stats,
    json_t *variable,
    const char *filename,
    uint64_t t,
    json_t *value
)
{
    char path[PATH_MAX];

    const char *directory = json_string_value(json_object_get(variable, "directory"));
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
   Calculate new value based in variable type
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
PUBLIC int wstats_add_value(
    json_t *stats,
    const char *variable_name,
    const char *group,
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
        return -1;
    }

    char full_variable_name[NAME_MAX];
    if(empty_string(group)) {
        snprintf(full_variable_name, sizeof(full_variable_name), "%s", variable_name);
    } else {
        snprintf(full_variable_name, sizeof(full_variable_name), "%s-%s", group, variable_name);
    }

    json_t *variables = kw_get_dict(stats, "variables", 0, KW_REQUIRED);
    json_t *variable = kw_get_dict(variables, full_variable_name, 0, 0);

    if(!variable) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Metric not configured",
            "directory",    "%s", kw_get_str(stats, "directory", "", KW_REQUIRED),
            "group",        "%s", group?group:"",
            "variable",     "%s", variable_name?variable_name:"",
            NULL
        );
        //log_debug_json(0, stats, "Metric not configured"); Too much information
        JSON_DECREF(jn_value);
        return -1;
    }

    uint64_t last_t = json_integer_value(json_object_get(variable, "last_t"));
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
            "group",        "%s", group?group:"",
            "variable",     "%s", variable_name?variable_name:"",
            "last_t",       "%ld", (unsigned long)last_t,
            "t",            "%ld", (unsigned long)t,
            NULL
        );
        JSON_DECREF(jn_value);
        return -1;
    }

    json_object_set_new(variable, "last_t", json_integer(t));

    json_t *metrics = json_object_get(variable, "metrics");
    char filename_[NAME_MAX];
    char stime_[NAME_MAX];
    int idx;
    json_t *metric;
    json_array_foreach(metrics, idx, metric) {
        const char *metric_type = json_string_value(json_object_get(metric, "metric_type"));
        const char *filename_mask = json_string_value(json_object_get(metric, "filename_mask"));
        const char *time_mask = json_string_value(json_object_get(metric, "time_mask"));

        const char *filename = json_string_value(json_object_get(metric, "filename"));
        const char *stime = json_string_value(json_object_get(metric, "stime"));

        render_mask(filename_, sizeof(filename_), filename_mask, TRUE, t);
        render_mask(stime_, sizeof(stime_), time_mask, TRUE, t);

        if(last_t == 0) {
            filename = filename_;
            stime = stime_;
            json_object_set_new(metric, "filename", json_string(filename_));
            json_object_set_new(metric, "stime", json_string(stime_));
        }

        /*
         *  Changed filename?
         */
        if(strcmp(filename, filename_)!=0) {
            // Close old file
            close_file(
                stats,
                variable,
                filename
            );

            // New file, delete if exists
            delete_file(
                stats,
                variable,
                filename_
            );
            json_object_set_new(metric, "filename", json_string(filename_));
        }

        /*
         *  Changed stime?
         */
        BOOL new_range = FALSE;
        if(strcmp(stime, stime_)!=0) {
            // New range
            if(last_t > 0) {
                // Save current acumulated time
                save_variable_value_to_file(
                    stats,
                    variable,
                    filename,
                    last_t,
                    json_object_get(metric, "value")
                );
                json_object_set_new(metric, "stime", json_string(stime_));
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
            json_object_get(metric, "value"),
            jn_value
        );
        json_object_set_new(metric, "value", new_value);
    }

    JSON_DECREF(jn_value);
    return 0;
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

    json_t *variables = kw_get_dict(stats, "variables", 0, KW_REQUIRED);
    const char *var_name; json_t *variable;
    json_object_foreach(variables, var_name, variable) {
        char path[PATH_MAX];
        json_t *jn_data = kw_get_propagated_key_values(variable, "id", keys);
        if(jn_data) {
            const char *directory = kw_get_str(variable, "directory", "", KW_REQUIRED);
            if(!empty_string(directory)) {
                snprintf(path, sizeof(path), "%s/%s", directory, "__partial_data__.json");
                json_dump_file(jn_data, path, JSON_INDENT(4));
            }
            json_decref(jn_data);
        }
    }

    const char *key;
    json_t *jn_value;
    json_t *file_opened_files = kw_get_dict(stats, "file_opened_files", 0, KW_REQUIRED);
    json_object_foreach(file_opened_files, key, jn_value) {
        FILE *file = (FILE *)(size_t)kw_get_int(file_opened_files, key, 0, KW_REQUIRED);
        if(file) {
            fflush(file);
        }
    }

    return 0;
}

/***************************************************************************
   Restore partial data
 ***************************************************************************/
PRIVATE int wstats_restore(
    json_t *variable
)
{
    char path[PATH_MAX];
    const char *directory = kw_get_str(variable, "directory", "", KW_REQUIRED);
    snprintf(path, sizeof(path), "%s/%s", directory, "__partial_data__.json");

    size_t flags = 0;
    json_error_t error;
    json_t *jn_data = json_load_file(path, flags, &error);
    if(!jn_data) {
        return -1;
    }
    int ret = kw_put_propagated_key_values(variable, "id", jn_data);
    return ret;
}
