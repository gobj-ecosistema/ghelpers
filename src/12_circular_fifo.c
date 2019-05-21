/***********************************************************************
 *              CFIFO.C
 *              FIFO queue implemented with a CircularFIFO buffer
 *              It's not thread-safe.
 *
 *              Copyright (c) 2013 Niyamaka.
 *              All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "12_circular_fifo.h"

/****************************************************************
 *         Constants
 ****************************************************************/

/****************************************************************
 *         Structures
 ****************************************************************/

/****************************************************************
 *         Data
 ****************************************************************/

/****************************************************************
 *         Prototypes
 ****************************************************************/

/***************************************************************************
 *  Crea un pkt de tamaoo de datos 'data_size' y
 *  una reserva para cabecera de 'header_size'.
 *  Retorna NULL if error
 ***************************************************************************/
CFIFO * cfifo_create(int data_size)
{
    CFIFO *cfifo;
    int total;

    total = sizeof(CFIFO) + data_size;

    cfifo = gbmem_malloc(total);
    if(!cfifo) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "bmem_malloc() return NULL",
            "sizeof",       "%d", sizeof(struct _CFIFO),
            "data_size",    "%d", data_size,
            "total",        "%d", total,
            NULL);
        return (CFIFO *)0;
    }

    /*---------------------------------*
     *   Inicializa atributos
     *---------------------------------*/
    cfifo->data_size = data_size;
    cfifo->start = 0;
    cfifo->count = 0;

    /*----------------------------*
     *   Retorna pointer
     *----------------------------*/
    return cfifo;
}

/***************************************************************************
 *    Elimina paquete
 ***************************************************************************/
void cfifo_remove(CFIFO *pkt)
{
    /*-----------------------*
     *  Libera la memoria
     *-----------------------*/
    gbmem_free(pkt);
}

/***************************************************************************
 *  WRITING: add `ln` bytes of `bf` to cicular fifo.
 *  If `ln` is 0 then put all data.
 *  If `ln` is greater than data len, then `ln` is limited to data len.
 *  Return number of written bytes.
 ***************************************************************************/
int cfifo_putdata(CFIFO *cfifo, const char *bf, int ln)
{
    int written;

    if (ln <= 0)
        ln = strlen(bf);

    if (ln == 0)
        return 0;

//    data = bf[0:ln]

    written = ln;

    if (ln > cfifo_freespace(cfifo)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "process",      "%s", get_process_name(),
            "hostname",     "%s", get_host_name(),
            "pid",          "%d", get_pid(),
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "no ENOUGH space",
            "ln",           "%d", ln,
            NULL);
    }

    int tail = (cfifo->start + cfifo->count) % cfifo->data_size; //  # write pointer
    int right_len = cfifo->data_size - tail;  // right zone
    cfifo->count += ln;

    if (ln <= right_len) {
        // enough with the right zone
        //self.data[tail:tail + ln] = data[:]
        memcpy(cfifo->data + tail, bf, ln);
    } else {
        // put maximum at the right zone
        //self.data[tail:tail + right_len] = data[:right_len]
        memcpy(cfifo->data + tail, bf, right_len);
        ln -= right_len;
    }

    // put the rest at the left zone
    //self.data[0:ln] = data[right_len:]
    memcpy(cfifo->data, bf + right_len, ln);

    return written;
}

/***************************************************************************
 *  READING : Pull 'ln' bytes.
 *  Return the pulled data.
 *  If ln is <= 0 then all data is pulled.
 *  If there is no enough 'ln' bytes, it return all the remaining.
 *  You MUST release returned data!!
 ***************************************************************************/
char *cfifo_getdata(CFIFO *cfifo, int ln)
{
    int right_len;
    char *data;

    if (ln <= 0 || ln > cfifo->count)
        ln = cfifo->count;

    data = gbmem_malloc(ln);

    right_len = cfifo->data_size - cfifo->start; // right zone (from start to end)

    if (ln <= right_len) {
        // enough with the right zone
        // data = self.data[self.start: self.start + ln]
        memcpy(data, cfifo->data + cfifo->start, ln);
        cfifo->count -= ln;
        cfifo->start += ln;
    } else {
        // get all the right zone
        //data = self.data[self.start: self.start + self.size]
        int ll = cfifo->data_size - cfifo->start;
        memcpy(data, cfifo->data + cfifo->start, ll);
        cfifo->count -= ln; //  # decrement now!
        ln -= ll;

        // add the rest of the left zone
        // data += self.data[0:ln]
        memcpy(data + ll, cfifo->data, ln);
        cfifo->start = ln;
    }

    return data;
}

/***************************************************************************
 *  WRITTING: Return number of free byte space to write.
 ***************************************************************************/
int cfifo_freespace(CFIFO *cfifo)
{
    int free_bytes = cfifo->data_size - cfifo->count;
    return free_bytes;
}
