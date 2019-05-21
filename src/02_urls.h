/****************************************************************************
 *              URLS.H
 *              Copyright (c) 2013, 2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/

#ifndef _URLS_H
#define _URLS_H 1

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
PUBLIC char *host2ip(const char *host);


#ifdef __cplusplus
}
#endif

#endif /* _URLS_H */
