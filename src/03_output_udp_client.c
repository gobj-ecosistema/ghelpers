/***********************************************************************
 *          OUTPUT_UDP_CLIENT.C
 *          Only Output client upd
 *
 *          Copyright (c) 2014, 2015 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#ifdef WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <process.h>
    #include <io.h>
#else
    #include <syslog.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
#endif
#include "03_output_udp_client.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 *  IPv4 maximum reassembly buffer size is 576, IPv6 has it at 1500.
 *  512 is a safe value to use across the internet.
 */
#define DEFAULT_UDP_FRAME_SIZE  1500
#define DEFAULT_BUFFER_SIZE     (256*1024L)


/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    DL_ITEM_FIELDS
    size_t udp_frame_size;
    size_t buffer_size;
    int output_format;
    int _s;
    uint32_t counter;
    struct sockaddr_in si_other;
    char schema[32], host[80], port[32];
    char bindip[80];
    char *buffer;
    char temp[8*1024];
    BOOL exit_on_fail;
} uclient_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE char __initialized__ = 0;
PRIVATE uint32_t sequence = 0;
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE dl_list_t dl_clients;
PRIVATE const char *months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
PRIVATE char __process_name__[64] = "output_udp_client";
PRIVATE char __hostname__[64] = {0};
PRIVATE int __pid__;


/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int _udpc_socket(uclient_t *uc);
PRIVATE int _udpc_write(uclient_t *uc, char *bf, int len);


/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int udpc_start_up(const char *process_name)
{
    if(__initialized__) {
        return -1;
    }
    if(process_name) {
        snprintf(__process_name__, sizeof(__process_name__), "%s", process_name);
    }
    gethostname(__hostname__, sizeof(__hostname__));
    __pid__ = (int)getpid();

    if (!atexit_registered) {
        atexit(udpc_end);
        atexit_registered = 1;
    }

    dl_init(&dl_clients);
    __initialized__ = TRUE;
    return 0;
}

/***************************************************************************
 *  Close all
 ***************************************************************************/
PUBLIC void udpc_end(void)
{
    uclient_t *uc;

    while((uc=dl_first(&dl_clients))) {
        udpc_close(uc);
    }
    __initialized__ = FALSE;
}

/***************************************************************************
 *  Return NULL on error
 ***************************************************************************/
PUBLIC udpc_t udpc_open(
    const char* url,
    const char *bindip,
    size_t bf_size,
    size_t udp_frame_size,
    ouput_format_t output_format,
    BOOL exit_on_fail)
{
    uclient_t *uc = 0;

    if(!__initialized__) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "udpc_open(): udpc not initialized"
        );
        return 0;
    }

    /*-------------------------------------*
     *          Check parameters
     *-------------------------------------*/
    if(!url || !*url) {
        print_error(
            PEF_SILENCE,
            "ERROR YUNETA",
            "udpc_open(): url EMPTY"
        );
        return 0;
    }
    if(udp_frame_size <= 0) {
        udp_frame_size = DEFAULT_UDP_FRAME_SIZE;
    }
    if(bf_size <=0) {
        bf_size = DEFAULT_BUFFER_SIZE;
    }
    if(output_format >= OUTPUT_LAST_ENTRY) {
        output_format = OUTPUT_FORMAT_YUNETA;
    }

    /*-------------------------------------*
     *          Alloc memory
     *-------------------------------------*/
    uc = malloc(sizeof(uclient_t));
    if(!uc) {
        print_error(
            PEF_ABORT,
            "ERROR YUNETA",
            "udpc_open(): No MEMORY % bytes",
            sizeof(uclient_t)
        );
        return 0;
    }
    memset(uc, 0, sizeof(uclient_t));
    if(bindip) {
        strncpy(uc->bindip, bindip, sizeof(uc->bindip)-1);
    }
    uc->buffer_size = bf_size;
    uc->udp_frame_size = udp_frame_size;
    uc->output_format = output_format;
    uc->exit_on_fail = exit_on_fail;

    uc->buffer = malloc(uc->buffer_size + 1);    // +1 for the terminator null
    if(!uc->buffer) {
        print_error(
            PEF_ABORT,
            "ERROR YUNETA",
            "udpc_open(): No MEMORY % bytes",
            bf_size
        );
        free(uc);
        return 0;
    }
    memset(uc->buffer, 0, uc->buffer_size + 1);

    /*-------------------------------------*
     *          Decode url
     *-------------------------------------*/
    int ret = parse_http_url(
        url,
        uc->schema,
        sizeof(uc->schema),
        uc->host,
        sizeof(uc->host),
        uc->port,
        sizeof(uc->port),
        FALSE
    );
    if(ret < 0) {
        print_error(
            PEF_SILENCE,
            "ERROR YUNETA",
            "parse_http_url(): FAILED"
        );
        free(uc->buffer);
        free(uc);
        return 0;
    }
    if(strcmp(uc->schema, "udp")!=0) {
        // no log error from user
        // ignore this error
    }

    /*-------------------------------------*
     *  Add to list and create socket
     *-------------------------------------*/
    dl_add(&dl_clients, uc);
    if(_udpc_socket(uc)<0) {
        udpc_close(uc);
        return 0;
    }

    return uc;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void udpc_close(udpc_t udpc)
{
    uclient_t *uc = udpc;

    if(!__initialized__ || !udpc) {
        return;
    }
    if(uc->_s > 0) {
        close(uc->_s);
        uc->_s = 0;
    }
    dl_delete(&dl_clients, uc, 0);
    free(uc->buffer);
    free(uc);
}

/***************************************************************************
 *  max len 64K bytes
 ***************************************************************************/
PUBLIC int udpc_write(udpc_t udpc, int priority, const char* bf_, size_t len)
{
    uclient_t *uc = udpc;
    const unsigned char* bf = (unsigned char*)bf_;
    if(!udpc || !bf) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "udpc_write(): udpc or bf NULL"
        );
        return -1;
    }

    size_t buffer_size = uc->buffer_size;
    size_t udp_frame_size = uc->udp_frame_size;

    if(len > buffer_size) {
        print_error(
            PEF_SILENCE,
            "ERROR YUNETA",
            "udpc_write(): bf too long"
        );
        return -1;
    }
    if(priority < 0 || priority > LOG_MONITOR) {
        priority = LOG_DEBUG;
    }

    /*
     *  Open or re-open the socket
     */
    if(uc->_s<=0) {
        if(_udpc_socket(uc) < 0) {
            // Error already logged
            return -1;
        }
    }

    /*
     *  Format
     */
    switch(uc->output_format) {
    case OUTPUT_FORMAT_RAW:
        memcpy(uc->buffer, bf, len);
        *(uc->buffer+len) = 0; // add terminator null
        len++;
        break;

    case OUTPUT_FORMAT_SYSLOG:
        if(priority <= LOG_DEBUG) {
            #ifdef __linux__
            syslog(priority, "%s", bf);
            #endif
        }
        return 0;

    case OUTPUT_FORMAT_LUMBERJACK:
        {
            /*
             *  Example lumberjack syslog message:
             *
             *      'Mar 24 12:01:34 localhost sshd[12590]: @cee:{
             *          "msg": "Accepted publickey for algernon from 127.0.0.1 port 55519 ssh2",
             *          "pid": "12590", "facility": "auth", "priority": "info",
             *          "program": "sshd", "uid": "0", "gid": "0",
             *          "host": "hadhodrond", "timestamp": "2012-03-24T12:01:34.236987887+0100"
             *      }'
             *
             *  Needed format: "Mmm dd hh:mm:ss" (dd is %2d)
             */
            char timestamp[20];
            struct tm *tm;
            time_t t;

            if(priority > LOG_DEBUG) {
                /*
                 *  Don't send my priorities to lumberjack
                 */
                return 0;
            }

            time(&t);
            tm = localtime(&t);

            snprintf(timestamp, sizeof(timestamp), "%s %2d %02d:%02d:%02d",
                months[tm->tm_mon],
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec
            );

            snprintf(
                uc->buffer,
                uc->buffer_size,
                "<%d>%s %s %s[%d]: @cee:%.*s",
                    (1 << 3) | priority, // Always LOG_USER
                    timestamp,
                    __hostname__,
                    __process_name__,
                    __pid__,
                    (int)len,
                    bf
            );
            len = strlen(uc->buffer);
            len++; // add final null
        }
        break;

    case OUTPUT_FORMAT_YUNETA:
    default:
        {
            uint32_t i, crc;

            unsigned char temp[12];
            snprintf( // NEW TODO
                (char *)temp,
                sizeof(temp),
                "%c%08"PRIX32,
                '0' + priority,
                sequence
            );
            crc = 0;
            for(i=0; i<strlen((char *)temp); i++) {
                crc += temp[i];
            }
            for(i=0; i<len; i++) {
                crc += bf[i];
            }
            snprintf( // NEW TODO
                uc->buffer,
                uc->buffer_size - 1,
                "%c%08"PRIX32"%s%08"PRIX32,
                '0' + priority,
                sequence,
                bf,
                crc
            );
            len = strlen(uc->buffer);
            len++; // add final null
            sequence++;
        }
        break;
    }

    register char *p = uc->buffer;
    while(len > 0) {
        int sent = MIN(udp_frame_size, len);
        if(_udpc_write(uc, p, sent)<0) {
            return -1;
        }
        len -= sent;
        p += sent;
    }

    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int udpc_fwrite(udpc_t udpc, int priority, const char *format, ...)
{
    uclient_t *uc = udpc;
    va_list ap;

    if(!udpc) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "udpc_write(): udpc NULL"
        );
        return -1;
    }
    va_start(ap, format);
    vsnprintf(
        uc->temp,
        sizeof(uc->temp),
        format,
        ap
    );
    va_end(ap);

    return udpc_write(uc, priority, uc->temp, strlen(uc->temp));
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _udpc_socket(uclient_t *uc)
{
    uc->_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
//     sequence = 0;
    if(uc->_s<=0) {
        print_error(
            uc->exit_on_fail,
            "ERROR YUNETA",
            "_udpc_socket(): socket(UDP) failed %d '%s'",
            errno,
            strerror(errno)
        );
        return -1;
    }

#ifdef __linux__
    int flags = fcntl(uc->_s, F_GETFL, 0);
    if(fcntl(uc->_s, F_SETFL, flags | O_NONBLOCK)<0) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "_udpc_socket(%s): fcntl() failed %d '%s'",
            uc->host,
            errno,
            strerror(errno)
        );
    }
#else
    u_long mode = 1;  // 1 to enable non-blocking socket
    ioctlsocket(uc->_s, FIONBIO, &mode);
#endif

    memset((char *) &uc->si_other, 0, sizeof(uc->si_other));
    uc->si_other.sin_family = AF_INET;
    uc->si_other.sin_port = htons(atoi(uc->port));

    //if (inet_aton(uc->host, &uc->si_other.sin_addr) == 0) {
    if (inet_pton(AF_INET, uc->host, &uc->si_other.sin_addr) <= 0) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "_udpc_socket(%s:%s): inet_pton() failed %d '%s'",
            uc->host,
            uc->port,
            errno,
            strerror(errno)
        );
        close(uc->_s);
        uc->_s = 0;
        return -1;
    }

    if(*uc->bindip) {
        static struct sockaddr_in si_bind;
        memset((char *) &si_bind, 0, sizeof(si_bind));
        si_bind.sin_family = AF_INET;

        //if (inet_aton(uc->bindip , &si_bind.sin_addr) == 0) {
        if (inet_pton(AF_INET, uc->bindip , &si_bind.sin_addr) <= 0) {
            print_error(
                PEF_CONTINUE,
                "ERROR YUNETA",
                "_udpc_socket(%s:%s): inet_pton() failed %d '%s'",
                uc->host,
                uc->port,
                errno,
                strerror(errno)
            );
        } else if (bind(uc->_s, (const struct sockaddr *)&si_bind, sizeof(si_bind))) {
            print_error(
                PEF_CONTINUE,
                "ERROR YUNETA",
                "_udpc_socket(%s:%s): bind() failed %d '%s'",
                uc->host,
                uc->port,
                errno,
                strerror(errno)
            );
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _udpc_write(uclient_t *uc, char *bf, int len)
{
    if(sendto(uc->_s, bf, len, 0, (struct sockaddr *)&uc->si_other, sizeof(uc->si_other))<0) {
        print_error(
            PEF_CONTINUE,
            "ERROR YUNETA",
            "_udpc_write(%s:%s): sendto() failed %d '%s'",
            uc->host,
            uc->port,
            errno,
            strerror(errno)
        );
        close(uc->_s);
        uc->_s = 0;
        return -1;
    }
    return 0;
}
