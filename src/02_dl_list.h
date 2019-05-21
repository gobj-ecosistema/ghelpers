/***********************************************************************
 *        INCLUDE OF 02_dl_list.h
 *
 *        Double link list functions.
 *        Independent functions of any C library.
 *
 *        Copyright (c) 1994-2016 Niyamaka.
 *        All Rights Reserved.
 ***********************************************************************/

#ifndef dl_list_s_H
#define dl_list_s_H 1

/*
 *  Dependencies
 */
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
#define DL_ITEM_FIELDS              \
    struct dl_list_s *__dl__;       \
    struct dl_item_s  *__next__;    \
    struct dl_item_s  *__prev__;    \
    size_t __id__;

typedef struct dl_item_s {
    DL_ITEM_FIELDS
} dl_item_t;


typedef struct dl_list_s {
    struct dl_item_s *head;
    struct dl_item_s *tail;
    size_t __itemsInContainer__;
    size_t __last_id__; // autoincremental, siempre.
} dl_list_t;

typedef void (*fnfree)(void *);

/*****************************************************************
 *     Prototypes
 *****************************************************************/

int dl_init(dl_list_t *dl);
void dl_flush(dl_list_t *dl, fnfree fnfree);

void * dl_find(dl_list_t *dl, void *item);          /* busca desde head */
void * dl_nfind(dl_list_t *dl, size_t nitem);       /* busca desde head por número, relative to 1 */
void * dl_find_back(dl_list_t *dl, void *item);     /* busca desde tail */

void * dl_first(dl_list_t *dl);
void * dl_last(dl_list_t *dl);
void * dl_next(void *curr);
void * dl_prev(void *curr);

size_t dl_id(void *item);

int dl_add(dl_list_t *dl, void *item);          /* add at end of list */
int dl_insert(dl_list_t *dl, void *item);       /* insert at head of list */
int dl_insert_before(dl_list_t *dl, void *curr, void *new_item);
int dl_insert_after(dl_list_t *dl, void *curr, void *new_item);
int dl_delete(dl_list_t *dl, void * curr, void (*fnfree)(void *));
int dl_delete_item(void * curr_, void (*fnfree)(void *));

size_t dl_index(dl_list_t *dl, void *item);     /* return list position of item */
size_t dl_size(dl_list_t *dl);                  /* return number of items in list */

dl_list_t *dl_dl_list(void *item);              /* return the dl_list_t of a item */

void dl_move(dl_list_t *destination, dl_list_t *source);

#ifdef __cplusplus
}
#endif

#endif
