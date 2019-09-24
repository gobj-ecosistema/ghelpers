/****************************************************************************
 *          WEBIX_HELPER.H
 *
 *          Helpers for the great webix framework
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/

#pragma once

#include <ghelpers.h>

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
    json_t *filter,  // owned
    const char *options // "permissive" "verbose"
);

#ifdef __cplusplus
}
#endif
