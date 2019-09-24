/***********************************************************************
 *          WEBIX_HELPER.C
 *
 *          Helpers for the great webix framework
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include "15_webix_helper.h"

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

 ***************************************************************************/
PRIVATE int _new_list_tree(
    json_t *list,
    json_t *node,  // NOT owned
    const char *hook,
    json_t *filter,  // NOT owned
    const char *options // "permissive" "verbose"
)
{
    BOOL verbose = (options && strstr(options, "verbose"))?TRUE:FALSE;

    int ret = 0;

    switch(json_typeof(node)) {
    case JSON_OBJECT:
        {
            /*
             *  Add current record
             */
            if(!kw_has_key(node, "id")) {
                const char *id; json_t *r;
                json_object_foreach(node, id, r) {
                    ret += _new_list_tree(
                        list,
                        r,
                        hook,
                        filter,
                        options
                    );
                }
                break;
            }

            json_t *new_record = json_object();
            json_array_append_new(list, new_record);

            json_object_set(
                new_record,
                "id",
                json_object_get(node, "id")
            );

            const char *field_name; json_t *f_v;
            json_object_foreach(filter, field_name, f_v) {
                const char *new_field_name = json_string_value(f_v);
                json_t *field_value = json_object_get(node, field_name);
                if(!field_value) {
                    if(verbose) {
                        log_error(0,
                            "gobj",         "%s", __FILE__,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                            "msg",          "%s", "filter_name not found in node",
                            "field_name",   "%s", field_name,
                            NULL
                        );
                    }
                    continue;
                }
                if(json_typeof(field_value)==JSON_OBJECT ||
                        json_typeof(field_value)==JSON_ARRAY) {
                    json_t *only_id_record = kwid_get_id_records(field_value);
                    json_object_set_new(
                        new_record,
                        !empty_string(new_field_name)?new_field_name:field_name,
                        only_id_record
                    );
                } else {
                    json_object_set(
                        new_record,
                        !empty_string(new_field_name)?new_field_name:field_name,
                        field_value
                    );
                }
            }


            if(options && strstr(options, "permissive")) {
                json_object_update_missing(new_record, node);
            }

            /*
             *  Next hook level
             */
            json_t *hook_data = json_object_get(node, hook);
            if(hook_data) {
                json_t *new_list = json_array();
                json_object_set_new(new_record, "data", new_list);
                ret += _new_list_tree(
                    new_list,
                    hook_data,
                    hook,
                    filter,
                    options
                );
            }
        }
        break;
    case JSON_ARRAY:
        {
            // TODO caso no probado
            int idx; json_t *r;
            json_array_foreach(node, idx, r) {
                ret += _new_list_tree(
                    list,
                    r,
                    hook,
                    filter,
                    options
                );
            }
        }
        break;
    default:
        if(options && strstr(options, "verbose")) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "????",
                NULL
            );
        }
        return -1;
    }

    return ret;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC json_t *webix_new_list_tree(
    json_t *tree,  // NOT owned
    const char *hook,
    json_t *filter,  // owned
    const char *options // "permissive" "verbose"
)
{
    if(!tree) {
        if(options && strstr(options, "verbose")) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "tree null",
                NULL
            );
        }
        JSON_DECREF(filter)
        return 0;
    }
    if(!(json_typeof(tree)==JSON_ARRAY || json_typeof(tree)==JSON_OBJECT)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tree must be a list or dict",
            NULL
        );
        JSON_DECREF(filter)
        return 0;
    }

    json_t *new_list = json_array();

    _new_list_tree(
        new_list,
        tree,  // NOT owned
        hook,
        filter,  // NOT owned
        options // "permissive" "verbose"
    );

    JSON_DECREF(filter)
    return new_list;
}


