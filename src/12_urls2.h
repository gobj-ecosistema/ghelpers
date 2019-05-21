/****************************************************************************
 *              URLS2
 *              Urls function using gbmem
 *
 *              Copyright (c) Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/

#ifndef _URLS2_H
#define _URLS2_H 1

/*
 *  Dependencies
 */
#include "11_gbmem.h"

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Constants
 *********************************************************************/
typedef enum {
    CODE_AS_RFC3986     = 0,
    CODE_AS_HTML5       = 1,
} code_url_as_t;

/*********************************************************************
 *      Structures
 *********************************************************************/

/*********************************************************************
 *      Prototypes
 *********************************************************************/
/*
 *  Encode string for use in url
 *  WARNING Remember gbmem_free() the return
 */
PUBLIC char *url_encode(const char *s, code_url_as_t coding); 

#ifdef __cplusplus
}
#endif


#endif

