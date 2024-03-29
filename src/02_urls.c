/****************************************************************************
 *              URLS.C
 *              Copyright (c) 2013, 2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include <string.h>
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
    http_parser_url_init(&u);

    if(host && host_size > 0) {
        host[0] = 0;
    }
    if(schema && schema_size > 0) {
        schema[0] = 0;
    }
    if(port && port_size > 0) {
        port[0] = 0;
    }

    int result = http_parser_parse_url(uri, strlen(uri), no_schema, &u);
    if (result != 0) { // WARNING with error return 1
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
    if(schema && schema_size > 0) {
        if(!no_schema) {
            ln = u.field_data[UF_SCHEMA].len;
            if(ln > 0) {
                if(ln >= schema_size) {
                    ln = schema_size - 1;
                    ret = -2;
                }
                memcpy(schema, uri + u.field_data[UF_SCHEMA].off, ln);
                schema[ln] = 0;
            }
        }
    }

    /*
     *  Host
     */
    if(host && host_size > 0) {
        ln = u.field_data[UF_HOST].len;
        if(ln > 0) {
            if(ln >= host_size) {
                ln = host_size - 1;
            }
            memcpy(host, uri + u.field_data[UF_HOST].off, ln);
            host[ln] = 0;
        }
    }

    /*
     *  Port
     */
    if(port && port_size > 0) {
        ln = u.field_data[UF_PORT].len;
        if(ln > 0) {
            if(ln >= port_size) {
                ln = port_size - 1;
            }
            memcpy(port, uri + u.field_data[UF_PORT].off, ln); port[ln]=0;
        }
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
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
)
{
    struct http_parser_url u;
    http_parser_url_init(&u);

    if(host) host[0] = 0;
    if(schema) schema[0] = 0;
    if(port) port[0] = 0;
    if(path) path[0] = 0;
    if(query) query[0] = 0;
    if(fragment) fragment[0] = 0;
    if(user_info) user_info[0] = 0;

    int result = http_parser_parse_url(uri, strlen(uri), no_schema, &u);
    if (result != 0) { // WARNING with error return 1
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
    if(schema && schema_size > 0) {
        if(!no_schema) {
            ln = u.field_data[UF_SCHEMA].len;
            if(ln > 0) {
                if(ln >= schema_size) {
                    ln = schema_size - 1;
                }
                memcpy(schema, uri + u.field_data[UF_SCHEMA].off, ln);
                schema[ln] = 0;
            }
        }
    }

    /*
     *  Host
     */
    if(host && host_size > 0) {
        ln = u.field_data[UF_HOST].len;
        if(ln > 0) {
            if(ln >= host_size) {
                ln = host_size - 1;
            }
            memcpy(host, uri + u.field_data[UF_HOST].off, ln);
            host[ln] = 0;
        }
    }

    /*
     *  Port
     */
    if(port && port_size > 0) {
        ln = u.field_data[UF_PORT].len;
        if(ln > 0) {
            if(ln >= port_size) {
                ln = port_size - 1;
            }
            memcpy(port, uri + u.field_data[UF_PORT].off, ln);
            port[ln] = 0;
        }
    }

    /*
     *  Path
     */
    if(path && path_size > 0) {
        ln = u.field_data[UF_PATH].len;
        if(ln > 0) {
            if(ln >= path_size) {
                ln = path_size - 1;
            }
            memcpy(path, uri + u.field_data[UF_PATH].off, ln);
            path[ln] = 0;
        }
    }

    /*
     *  Query
     */
    if(query && query_size > 0) {
        ln = u.field_data[UF_QUERY].len;
        if(ln > 0) {
            if(ln >= query_size) {
                ln = query_size - 1;
            }
            memcpy(query, uri + u.field_data[UF_QUERY].off, ln);
            query[ln] = 0;
        }
    }

    /*
     *  Fragment
     */
    if(fragment && fragment_size > 0) {
        ln = u.field_data[UF_FRAGMENT].len;
        if(ln > 0) {
            if(ln >= fragment_size) {
                ln = fragment_size - 1;
            }
            memcpy(fragment, uri + u.field_data[UF_FRAGMENT].off, ln);
            fragment[ln] = 0;
        }
    }

    /*
     *  User_info
     */
    if(user_info && user_info_size > 0) {
        ln = u.field_data[UF_USERINFO].len;
        if(ln > 0) {
            if(ln >= user_info_size) {
                ln = user_info_size - 1;
            }
            memcpy(user_info, uri + u.field_data[UF_USERINFO].off, ln);
            user_info[ln] = 0;
        }
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int parse_partial_http_url(
    const char *uri,
    char *schema, int schema_size,
    char *host, int host_size,
    char *port, int port_size,
    char *path, int path_size,
    char *query, int query_size,
    BOOL no_schema
)
{
    struct http_parser_url u;
    http_parser_url_init(&u);

    if(host) host[0] = 0;
    if(schema) schema[0] = 0;
    if(port) port[0] = 0;
    if(path) path[0] = 0;
    if(query) query[0] = 0;

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
    if(schema && schema_size > 0) {
        if(!no_schema) {
            ln = u.field_data[UF_SCHEMA].len;
            if(ln > 0) {
                if(ln >= schema_size) {
                    ln = schema_size - 1;
                }
                memcpy(schema, uri + u.field_data[UF_SCHEMA].off, ln);
                schema[ln] = 0;
            }
        }
    }

    /*
     *  Host
     */
    if(host && host_size > 0) {
        ln = u.field_data[UF_HOST].len;
        if(ln > 0) {
            if(ln >= host_size) {
                ln = host_size - 1;
            }
            memcpy(host, uri + u.field_data[UF_HOST].off, ln);
            host[ln] = 0;
        }
    }

    /*
     *  Port
     */
    if(port && port_size > 0) {
        ln = u.field_data[UF_PORT].len;
        if(ln > 0) {
            if(ln >= port_size) {
                ln = port_size - 1;
            }
            memcpy(port, uri + u.field_data[UF_PORT].off, ln);
            port[ln] = 0;
        }
    }

    /*
     *  Path
     */
    if(path && path_size > 0) {
        ln = u.field_data[UF_PATH].len;
        if(ln > 0) {
            if(ln >= path_size) {
                ln = path_size - 1;
            }
            memcpy(path, uri + u.field_data[UF_PATH].off, ln);
            path[ln] = 0;
        }
    }

    /*
     *  Query
     */
    if(query && query_size > 0) {
        ln = u.field_data[UF_QUERY].len;
        if(ln > 0) {
            if(ln >= query_size) {
                ln = query_size - 1;
            }
            memcpy(query, uri + u.field_data[UF_QUERY].off, ln);
            query[ln] = 0;
        }
    }

    return ret;
}
