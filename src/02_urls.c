/****************************************************************************
 *              URLS.C
 *              Copyright (c) 2013, 2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <netdb.h>
#include "02_urls.h"

/***************************************************************************
 *  Decode a url
 ***************************************************************************/
PUBLIC int parse_http_url(
    const char *uri,
    char *schema, int schema_size,
    char *host, int host_size,
    char *port, int port_size,
    BOOL no_schema)
{
    struct http_parser_url u;

    if(host) host[0] = 0;
    if(schema) schema[0] = 0;
    if(port) port[0] = 0;

    int result = http_parser_parse_url(uri, strlen(uri), no_schema, &u);
    if (result != 0) {
        print_error(
            PEF_SILENCE,
            "ERROR YUNETA",
            "http_parser_parse_url(%s): FAILED",
            uri
        );
        return -1;
    }

    int ret=0;
    int ln;

    /*
     *  Schema
     */
    if(schema) {
        if(!no_schema) {
            ln = u.field_data[UF_SCHEMA].len;
            if(ln >= schema_size) {
                ln = schema_size - 1;
                ret = -3;
            }
            memcpy(schema, uri + u.field_data[UF_SCHEMA].off, ln); schema[ln]=0;
        }
    }

    /*
     *  Host
     */
    if(host) {
        ln = u.field_data[UF_HOST].len;
        if(ln >= host_size) {
            ln = host_size - 1;
            ret = -2;
        }
        memcpy(host, uri + u.field_data[UF_HOST].off, ln); host[ln]=0;
    }

    /*
     *  Port
     */
    if(port) {
        ln = u.field_data[UF_PORT].len;
        if(ln >= port_size) {
            ln = port_size - 1;
            ret = -4;
        }
        memcpy(port, uri + u.field_data[UF_PORT].off, ln); port[ln]=0;
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *host2ip(const char *host)
{
    struct hostent *lh = gethostbyname(host);

    if (lh)
        return lh->h_name;
    else
        return 0;
}
