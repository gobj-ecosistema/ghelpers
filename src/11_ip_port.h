/****************************************************************************
 *          IP_PORT.H
 *          Helpers for ip4/ip6
 *
 *          Copyright (c) 2014 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <uv.h>
#include "10_glogger.h"

#ifdef __cplusplus
extern "C"{
#endif


/*********************************************************************
 *      Structures
 *********************************************************************/
typedef struct {
    char is_ip6;
    union {
        struct sockaddr ip;
        struct sockaddr_in ip4;
        struct sockaddr_in6 ip6;
    } sa;
} ip_port;

/*********************************************************************
 *      Prototypes
 *********************************************************************/
PUBLIC int get_tcp_sock_name(uv_tcp_t *uv_socket, ip_port *ipp);
PUBLIC int get_tcp_peer_name(uv_tcp_t *uv_socket, ip_port *ipp);
PUBLIC int get_udp_sock_name(uv_udp_t *uv_socket, ip_port *ipp);
PUBLIC int get_ipp_url(ip_port *ipp, char *url, int urlsize);

#ifdef __cplusplus
}
#endif
