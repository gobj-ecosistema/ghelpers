/****************************************************************************
 *          IP_PORT.C
 *          Helpers for ip4/ip6
 *
 *          Copyright (c) 2014 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <netinet/in.h>
#include <string.h>
#include "11_ip_port.h"

/***************************************************************************
 *  Get tcp sockname
 ***************************************************************************/
PUBLIC int get_tcp_sock_name(uv_tcp_t *uv_socket, ip_port *ipp)
{
    int len = sizeof(ipp->sa.ip);
    int err = uv_tcp_getsockname(
        uv_socket,
        &ipp->sa.ip,
        &len
    );
    if(err!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "uv_tcp_getsockname() FAILED",
            "uv_error",     "%s", uv_err_name(err),
            NULL
        );
        memset(&ipp->sa, 0, sizeof(ipp->sa));
        return -1;
    }
    if(len == sizeof(struct sockaddr_in))
        ipp->is_ip6 = 0;
    else
        ipp->is_ip6 = 1;

    return 0;
}

/***************************************************************************
 *  Get tcp peername
 ***************************************************************************/
PUBLIC int get_tcp_peer_name(uv_tcp_t *uv_socket, ip_port *ipp)
{
    int len = sizeof(ipp->sa.ip);
    int err = uv_tcp_getpeername(
        uv_socket,
        &ipp->sa.ip,
        &len
    );
    if(err!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "uv_tcp_getpeername() FAILED",
            "uv_error",     "%s", uv_err_name(err),
            NULL
        );
        memset(&ipp->sa, 0, sizeof(ipp->sa));
        return -1;
    }
    if(len == sizeof(struct sockaddr_in))
        ipp->is_ip6 = 0;
    else
        ipp->is_ip6 = 1;

    return 0;
}

/***************************************************************************
 *  Get udp sockname
 ***************************************************************************/
PUBLIC int get_udp_sock_name(uv_udp_t *uv_socket, ip_port *ipp)
{
    int len = sizeof(ipp->sa.ip);
    int err = uv_udp_getsockname(
        uv_socket,
        &ipp->sa.ip,
        &len
    );
    if(err!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "uv_udp_getsockname() FAILED",
            "uv_error",     "%s", uv_err_name(err),
            NULL
        );
        memset(&ipp->sa, 0, sizeof(ipp->sa));
        return -1;
    }
    if(len == sizeof(struct sockaddr_in))
        ipp->is_ip6 = 0;
    else
        ipp->is_ip6 = 1;

    return 0;
}

/***************************************************************************
 *  Get url of sockaddr (without schema)
 ***************************************************************************/
PUBLIC int get_ipp_url(ip_port *ipp, char *url, int urlsize)
{
    char addr[50] = {'\0'};
    int port;

    if(ipp->is_ip6) {
        uv_ip6_name(&ipp->sa.ip6, addr, sizeof(addr));
        port = ntohs(ipp->sa.ip6.sin6_port);
        snprintf(url, urlsize, "[%s]:%d", addr, port);
    } else {
        uv_ip4_name(&ipp->sa.ip4, addr, sizeof(addr));
        port = ntohs(ipp->sa.ip4.sin_port);
        snprintf(url, urlsize, "%s:%d", addr, port);
    }
    return 0;
}
