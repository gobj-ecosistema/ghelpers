/****************************************************************************
 *              URLS.H
 *              Copyright (c) 2013, 2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

/*
 *  Dependencies
 */
#include "00_http_parser.h"
#include "01_print_error.h"

#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Constants
 *****************************************************************/

/*****************************************************************
 *     Structures
 *****************************************************************/

/*****************************************************************
 *     Prototypes
 *****************************************************************/
/*
 *  If error return a negative value.
 */
PUBLIC int parse_http_url(
    const char *uri,
    char *schema, int schema_size,
    char *host, int host_size,
    char *port, int port_size,
    BOOL no_schema
);

PUBLIC int parse_full_http_url(
    const char *uri,
    char *schema, int schema_size,
    char *host, int host_size,
    char *port, int port_size,
    char *path, int path_size,
    char *query, int query_size,
    char *fragment, int fragment_size,
    char *user_info, int user_info_size,
    BOOL no_schema
);

PUBLIC int parse_partial_http_url(
    const char *uri,
    char *schema, int schema_size,
    char *host, int host_size,
    char *port, int port_size,
    char *path, int path_size,
    char *query, int query_size,
    BOOL no_schema
);

#ifdef __cplusplus
}
#endif
