/****************************************************************************
 *              JSON_XML.H
 *              Copyright (c) 2018 Niyamaka.
 *
 *  Conversión de xml a json,
 *  no procesa attributos (<tag attr="1">text</tag>), solo text.
 *
 *              All Rights Reserved.
 ****************************************************************************/

#ifndef _JSON_XML_H
#define _JSON_XML_H 1

#include <jansson.h>
/*
 *  Dependencies
 */
#include "20_gbuffer.h"

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
PUBLIC void *xml_init(
    void *user_data,
    void (*startElement)(
        void *user_data,
        const char *name,
        const char **attrs  // Not implemented
    ),
    void (*endElement)(void *user_data, const char *name),
    void (*charData)(void *user_data, const char *s),
    void (*publishJson)(
        void *user_data,
        json_t *jn  // must be owned
    )
);
PUBLIC int xml_end(void *parser);
PUBLIC int xml_reset(void *parser);
PUBLIC int xml_parse(
    void *parser,
    char *bf,
    int len
);

#ifdef __cplusplus
}
#endif

#endif /* _JSON_XML_H */
