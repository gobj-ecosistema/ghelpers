/***********************************************************************
 *          STATS_RD.C
 *
 *          Simple statistics handler, reader
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <unistd.h>
#include <errno.h>
#include "20_stats_rd.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define INSIDE(x, x1, x2) ((x>=x1 && x<=x2)?1:0)

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
    Example of __metric__.json
    {
        "metric_name": "dbwrite-queue-0",
        "version": "1",
        "group": "queues",
        "types": [
            {
                "id": "last_week_in_seconds",
                "metric_type": "",
                "value_type": 0.0,
                "filename_mask": "WDAY",
                "time_mask": "SEC"
            }
        ],
        "database": "dbwriter^10220"
    }
***************************************************************************/
PRIVATE BOOL read_metric_cb(
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[256]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    json_t *stats = user_data;

    json_t *jn_metric = load_json_from_file(
        directory,
        "__metric__.json",
        0
    );
    if(!jn_metric) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot load metric json file",
            "path",         "%s", fullpath,
            NULL
        );
        return TRUE; // to continue
    }

    json_t *metrics = kw_get_dict(stats, "metrics", 0, KW_REQUIRED);
    json_t *groups = kw_get_list(stats, "groups", 0, KW_REQUIRED);

    const char *database = kw_get_str(jn_metric, "database", "", KW_REQUIRED);
    const char *metric_name = kw_get_str(jn_metric, "metric_name", "", KW_REQUIRED);
    const char *group = kw_get_str(jn_metric, "group", 0, 0);
    json_t *types = kw_get_list(jn_metric, "types", 0, KW_REQUIRED);

    char id[NAME_MAX];
    if(json_array_size(groups)>0) {
        // Add only if his group is in the list
        if(!empty_string(group) && json_str_in_list(groups, group, TRUE)) {
            snprintf(id, sizeof(id), "%s.%s.%s",
                group,
                database,
                metric_name
            );
            json_object_set(metrics, id, types);
        }
    } else {
        snprintf(id, sizeof(id), "%s.%s.%s",
            group,
            database,
            metric_name
        );
        json_object_set(metrics, id, types);
    }

    JSON_DECREF(jn_metric);
    return TRUE; // to continue
}

PRIVATE int load_metrics(json_t *stats)
{
    return walk_dir_tree(
        kw_get_str(stats, "directory", "", KW_REQUIRED),
        "__metric__\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        read_metric_cb,
        stats
    );

}

/***************************************************************************
   Open simple stats, as client. Only read.
 ***************************************************************************/
PUBLIC json_t *rstats_open(
    json_t *jn_stats  // owned
)
{
    const char *path = kw_get_str(jn_stats, "path", "", 0);
    json_t *groups = kw_get_dict_value(jn_stats, "groups", 0, 0); // Can be string or [string]
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

    json_t *stats = json_object();
    kw_get_str(stats, "directory", path, KW_CREATE);
    kw_get_dict(stats, "metrics", json_object(), KW_CREATE);
    kw_get_dict(stats, "file_opened_files", json_object(), KW_CREATE);
    kw_get_dict(stats, "fd_opened_files", json_object(), KW_CREATE);

    // Add group decoupled
    if(!groups) {
        json_object_set_new(stats, "groups", json_array());
    } else {
        if(json_is_string(groups)) {
            json_t *g = json_array();
            const char *s = json_string_value(groups);
            if(!empty_string(s)) {
                json_array_append_new(g, json_string(s));
            }
            json_object_set_new(stats, "groups", g);
        } else if(json_is_array(groups)) {
            json_object_set_new(stats, "groups", json_deep_copy(groups));
        } else {
            json_object_set_new(stats, "groups", json_array());
        }
    }

    load_metrics(stats);

    JSON_DECREF(jn_stats);

    return stats;
}

/***************************************************************************
   Close simple stats
 ***************************************************************************/
PUBLIC void rstats_close(
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
 *  Get first and last record
 ***************************************************************************/
PRIVATE json_t *check_file_ranges(const char *fullpath)
{
    FILE *file = fopen64(fullpath, "r");
    if(!file) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open file",
            "path",         "%s", fullpath,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return 0;
    }

    uint64_t size = filesize2(fileno(file));
    if((size % sizeof(stats_record_t)) !=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "File Size NOT MATCH with stats_record_t",
            "path",         "%s", fullpath,
            "size",         "%lu", (unsigned long)size,
            "errno",        "%s", strerror(errno),
            NULL
        );
    }
    uint64_t last_record = size/sizeof(stats_record_t) - 1;
    uint64_t last_record_offset = last_record * sizeof(stats_record_t);

    stats_record_t first_stats_record;
    stats_record_t last_stats_record;

    if(fread(&first_stats_record, sizeof(stats_record_t), 1, file)!=1) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read first first_stats_record",
            "path",         "%s", fullpath,
            "errno",        "%s", strerror(errno),
            NULL
        );
        fclose(file);
        return 0;
    }

    if(fseeko64(file, last_record_offset, SEEK_SET)<0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot fseek to last_stats_record",
            "path",         "%s", fullpath,
            "errno",        "%s", strerror(errno),
            NULL
        );
        fclose(file);
        return 0;
    }

    if(fread(&last_stats_record, sizeof(stats_record_t), 1, file)!=1) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read last last_stats_record",
            "path",         "%s", fullpath,
            "errno",        "%s", strerror(errno),
            NULL
        );
        fclose(file);
        return 0;
    }

    fclose(file);

    json_t *jn_ranges = json_object();
    json_object_set_new(jn_ranges, "fr_t", json_integer(first_stats_record.t));
    json_object_set_new(jn_ranges, "to_t", json_integer(last_stats_record.t));

    struct tm *tm;
    char from_d[80];
    char to_d[80];
    tm = gmtime((time_t *)&first_stats_record.t);
    tm2timestamp(from_d, sizeof(from_d), tm);
    tm = gmtime((time_t *)&last_stats_record.t);
    tm2timestamp(to_d, sizeof(to_d), tm);

    json_object_set_new(jn_ranges, "fr_d", json_string(from_d));
    json_object_set_new(jn_ranges, "to_d", json_string(to_d));

    json_object_set_new(jn_ranges, "file", json_string(fullpath));

    return jn_ranges;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL read_files_cb(
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[256]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    json_t *jn_list = user_data;

    json_t *jn_ranges = check_file_ranges(fullpath);
    if(jn_ranges) {
        //json_array_append_new(jn_list, jn_ranges);
        kw_insert_ordered_record(jn_list, "fr_t", jn_ranges);
    }
    return TRUE; // to continue
}

PRIVATE int check_files(const char *path, const char *pattern, json_t *jn_list)
{
    return walk_dir_tree(
        path,
        pattern,
        WD_MATCH_REGULAR_FILE,
        read_files_cb,
        jn_list
    );

}

/***************************************************************************
    Get metrics
 ***************************************************************************/
PUBLIC json_t *rstats_metrics(
    json_t *stats
)
{
    json_t *jn_limits = json_object();

    const char *directory = kw_get_str(stats, "directory", "", KW_REQUIRED);

    json_t *metrics = kw_get_dict(stats, "metrics", json_object(), KW_REQUIRED);
    const char *name;
    json_t *jn_list;
    json_object_foreach(metrics, name, jn_list) {
        json_t *metric = kw_get_dict(jn_limits, name, json_object(), KW_CREATE);
        int idx;
        json_t *jn_metric;
        json_array_foreach(jn_list, idx, jn_metric) {
            const char *id = kw_get_str(jn_metric, "id", "", KW_REQUIRED);
            const char *metric_type = kw_get_str(jn_metric, "metric_type", "", KW_REQUIRED);
            BOOL real = json_is_real(kw_get_dict_value(jn_metric, "value_type", 0, KW_REQUIRED))?1:0;
            const char *time_mask_orig = kw_get_str(jn_metric, "time_mask", "MIN", KW_REQUIRED);

            char subdir[PATH_MAX];
            snprintf(
                subdir,
                sizeof(subdir),
                "%s/%s",
                directory,
                name
            );
            if(!is_directory(subdir)) {
                continue;
            }
            if(empty_string(id)) {
                continue;
            }
            json_t *jn_id = kw_get_dict(metric, id, json_object(), KW_CREATE);
            kw_get_str(jn_id, "metric_id", id, KW_CREATE);
            kw_get_str(jn_id, "variable", name, KW_CREATE);
            kw_get_str(
                jn_id,
                "period",
                kw_get_str(jn_metric, "filename_mask", "mon", KW_REQUIRED),
                KW_CREATE
            );
            kw_get_str(jn_id, "units", time_mask_orig, KW_CREATE);
            kw_get_str(jn_id, "compute", metric_type, KW_CREATE);
            kw_get_bool(jn_id, "real", real, KW_CREATE);
            json_t *list = kw_get_list(jn_id, "data", json_array(), KW_CREATE);
            check_files(subdir, id, list);
        }
    }

    return jn_limits;
}

/***************************************************************************
    List variables of `metrics`
 ***************************************************************************/
PUBLIC json_t *rstats_list_variables(
    json_t *metrics
)
{
    json_t *jn_list = json_array();
    const char *var;
    json_t *jn_v;
    json_object_foreach(metrics, var, jn_v) {
        json_array_append_new(jn_list, json_string(var));
    }
    return jn_list;
}

/***************************************************************************
    List variables of `metrics`
 ***************************************************************************/
PUBLIC json_t *rstats_list_limits(
    json_t *metrics,
    const char *variable
)
{
    json_t *limits = json_object();
    if(!empty_string(variable)) {
        if(!kw_has_key(metrics, variable)) {
            return limits;
        }
    }

    const char *var;
    json_t *jn_v;
    json_object_foreach(metrics, var, jn_v) {
        json_t *jn_var = 0;
        if(empty_string(variable)) {
            jn_var = kw_get_dict(limits, var, json_object(), KW_CREATE);
        } else if(strcasecmp(variable, var)!=0) {
            continue;
        }

        const char *metr;
        json_t *jn_metr;
        json_object_foreach(jn_v, metr, jn_metr) {
            const char *units = kw_get_str(jn_metr, "units", "", KW_REQUIRED);
            json_t *jn_limits;
            if(jn_var) {
                jn_limits = kw_get_list(jn_var, units, json_array(), KW_CREATE);
            } else {
                jn_limits = kw_get_list(limits, units, json_array(), KW_CREATE);
            }

            json_t *jn_data = kw_get_list(jn_metr, "data", 0, KW_REQUIRED);
            if(json_array_size(jn_data)) {
                int items = json_array_size(jn_data);
                json_t *jn_first = json_array_get(jn_data, 0);
                json_t *jn_last = json_array_get(jn_data, items-1);
                json_array_append_new(
                    jn_limits,
                    kw_get_dict_value(jn_first, "fr_t", 0, KW_REQUIRED)
                );
                json_array_append_new(
                    jn_limits,
                    kw_get_dict_value(jn_last, "to_t", 0, KW_REQUIRED)
                );
            }
        }
    }

    // WARNING Decoupled return.
    json_t *r = json_deep_copy(limits);
    json_decref(limits);
    return r;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *find_metric_by_units(
    json_t *metrics,
    const char *variable,
    const char *units,
    BOOL verbose
)
{
    json_t *jn_variable = kw_get_dict(metrics, variable, 0, 0);
    if(!variable) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "variable NOT FOUND",
                "variable",     "%s", variable?variable:"",
                NULL
            );
        }
        return 0;
    }

    const char *key;
    json_t *jn_value;
    json_object_foreach(jn_variable, key, jn_value) {
        const char *units_ = json_string_value(json_object_get(jn_value, "units"));
        if(strcasecmp(units, units_)==0) {
            // WARNING Decoupled return.
            return json_deep_copy(jn_value);
        }
    }

    if(verbose) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "variable NOT FOUND",
            "variable",     "%s", variable?variable:"",
            NULL
        );
    }

    return 0;
}

/***************************************************************************
    Get metric
 ***************************************************************************/
PUBLIC json_t *rstats_metric(
    json_t *metrics,
    //const char *group, // TODO add group option
    const char *variable,
    const char *metric_name,
    const char *units,
    BOOL verbose
)
{
    json_t *jn_variable = kw_get_dict(metrics, variable, 0, 0);
    if(!variable) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "variable NOT FOUND",
                "variable",     "%s", variable?variable:"",
                NULL
            );
        }
        return 0;
    }

    json_t *metric = 0;
    if(!empty_string(units)) {
        metric = find_metric_by_units(metrics, variable, units, TRUE);
        if(metric) {
            return metric;
        }
    }
    metric = json_object_get(jn_variable, metric_name);
    if(!metric) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "process",      "%s", get_process_name(),
                "hostname",     "%s", get_host_name(),
                "pid",          "%d", get_pid(),
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "metric NOT FOUND",
                "variable",     "%s", variable?variable:"",
                "metric",       "%s", metric_name?metric_name:"",
                NULL
            );
        }
        return 0;
    }

    // WARNING Decoupled return.
    return json_deep_copy(metric);
}

/***************************************************************************
    WARNING remember free return
 ***************************************************************************/
PRIVATE stats_record_t *load_data(const char *filename, uint64_t *n_records_, BOOL real)
{
    *n_records_ = 0;

    FILE *file = fopen64(filename, "r");
    if(!file) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open file",
            "filename",     "%s", filename,
            NULL
        );
        return 0;
    }

    uint64_t size = filesize2(fileno(file));
    uint64_t n_records = size/sizeof(stats_record_t);

    stats_record_t *records = gbmem_malloc(size);
    if(!records) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Not enough memory to load stats",
            "filename",     "%s", filename,
            "size",         "%ld", (unsigned long)size,
            NULL
        );
        fclose(file);
        return 0;
    }

    if(fread(records, sizeof(stats_record_t), n_records, file)!=n_records) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read all stats records",
            "filename",     "%s", filename,
            "size",         "%ld", (unsigned long)size,
            NULL
        );
        free(records);
        fclose(file);
        return 0;
    }

    *n_records_ = n_records;

    fclose(file);
    return records;
}

/***************************************************************************

 ***************************************************************************/
PRIVATE int add_data(
    json_t *jn_file,
    uint64_t from_t,
    uint64_t to_t,
    json_t *data,
    BOOL real)
{
    const char *filename = kw_get_str(jn_file, "file", "", KW_REQUIRED);

    uint64_t n_records;
    stats_record_t *records = load_data(filename, &n_records, real);
    stats_record_t *p = records;
    for(uint64_t i=0; i<n_records; i++, p++) {
        if(INSIDE(p->t, from_t, to_t)) {
            json_t *jn_d = json_object();
            json_object_set_new(jn_d, "t", json_integer(p->t));

            char d[80];
            struct tm *tm = gmtime((time_t *)&p->t);
            tm2timestamp(d, sizeof(d), tm);
            json_object_set_new(jn_d, "t2", json_string(d));

            if(real) {
                json_object_set_new(jn_d, "y", json_real(p->v.d));
            } else {
                json_object_set_new(jn_d, "y", json_integer(p->v.i64));
            }
            json_array_append_new(data, jn_d);
        }
    }
    gbmem_free(records);

    return 0;
}

/***************************************************************************
    Get stats
 ***************************************************************************/
PUBLIC json_t *rstats_get_data(
    json_t *metric,
    uint64_t from_t,
    uint64_t to_t
)
{
    json_t *data = json_array();
    if(!metric) {
        return data;
    }
    BOOL real = kw_get_bool(metric, "real", 0, KW_REQUIRED);
    json_t *jn_data = kw_get_list(metric, "data", 0, KW_REQUIRED);

    int idx;
    json_t *jn_file;
    json_array_foreach(jn_data, idx, jn_file) {
        json_int_t from_t_ = kw_get_int(jn_file, "fr_t", 0, KW_REQUIRED);
        json_int_t to_t_ = kw_get_int(jn_file, "to_t", 0, KW_REQUIRED);

        if(INSIDE(from_t_, from_t, to_t) || INSIDE(to_t_, from_t, to_t)) {
            add_data(jn_file, from_t, to_t, data, real);
        }
    }

    return data;
}
