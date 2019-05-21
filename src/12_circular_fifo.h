/****************************************************************************
 *              CIRCULAR_FIFO.H
 *              FIFO queue implemented with a CircularFIFO buffer
 *              It's not thread-safe.
 *
 *              Copyright (c) 1999-2013 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/

#ifndef _CIRCULAR_FIFO_H
#define _CIRCULAR_FIFO_H 1

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

/*********************************************************************
 *      Structures
 *********************************************************************/
typedef struct _CFIFO {
    int     data_size;  /* no bytes allocados en 'data' */
    int     start;
    int     count;
    /*
     *  Datos allocados dinamicamente (header_size + data_size)
     */
    uint8_t data[1];
} CFIFO;

/*********************************************************************
 *      Prototypes
 *********************************************************************/
CFIFO * cfifo_create(int data_size);
void cfifo_remove(CFIFO *cfifo);
int  cfifo_putdata(CFIFO *cfifo, const char *bf, int ln);
char *cfifo_getdata(CFIFO *cfifo, int ln);  // You MUST release returned data!!
int  cfifo_freespace(CFIFO *cfifo);

#ifdef __cplusplus
}
#endif


#endif

