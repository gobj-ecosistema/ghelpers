/****************************************************************************
 *          WEBIX_HELPER.H
 *
 *          Helpers for the great webix framework
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <jansson.h>

/*
 *  Dependencies
 */
#include "14_kw.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/

/**rst**

**rst**/
PUBLIC json_t *webix_new_list_tree(
    json_t *tree,  // NOT owned
    const char *hook,
    const char *renamed_hook, // if not empty change the hook name in the result
    json_t *filter,  // owned (fields of tree'records to include, and possible field rename)
    const char *options // "permissive" "verbose"
);

#ifdef __cplusplus
}
#endif
