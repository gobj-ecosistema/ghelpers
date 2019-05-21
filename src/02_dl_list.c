/***********************************************************************
 *        dl_list.c
 *
 *        Double link list functions.
 *        Independent functions of any C library.
 *
 *        Copyright (c) 1994-2016 Niyamaka.
 *        All Rights Reserved.
 ***********************************************************************/
#include <stdlib.h>
#include "02_dl_list.h"

/*********************************************************
 *      Constants
 *********************************************************/

/*********************************************************
 *      Data
 *********************************************************/


/***************************************************************
 *      Initialize double list
 ***************************************************************/
int dl_init(dl_list_t *dl)
{
    if(dl->head || dl->tail || dl->__itemsInContainer__) {
        print_error(
            PEF_ABORT,
            "YUNETA ERROR",
            "dl_init(): Wrong dl_list_t, MUST be empty."
            " %s %s %d",
            get_host_name(),
            get_process_name(),
            get_pid()
        );
    }
    dl->head = 0;
    dl->tail = 0;
    dl->__itemsInContainer__ = 0;
    return 0;
}

/***************************************************************
 *      Flush double list. (Remove all items).
 ***************************************************************/
void dl_flush(dl_list_t *dl, void (*fnfree)(void *))
{
    register dl_item_t *first;

    first = dl_first(dl);
    while(first) {
        dl_delete(dl, first, fnfree);
        first = dl_first(dl);
    }
    if(dl->head || dl->tail || dl->__itemsInContainer__) {
        print_error(
            PEF_ABORT,
            "YUNETA ERROR",
            "dl_flush(): Wrong dl_list_t, MUST be empty."
            " %s %s %d",
            get_host_name(),
            get_process_name(),
            get_pid()
        );
    }
}

/***************************************************************
 *      dl_find - find forward
 ***************************************************************/
void * dl_find(dl_list_t *dl, void *item)
{
    register dl_item_t * curr;

    curr = dl->head;

    while(curr != 0)  {
        if(curr==item)
            return item;
        curr = curr->__next__;
    }
    return (void *)0; /* not found */
}

/***************************************************************
 *      dl_find - find forward the item in position `nitem`
 ***************************************************************/
void * dl_nfind(dl_list_t *dl, size_t nitem)
{
    register dl_item_t * curr;
    register size_t i;

    if(nitem == 0 || nitem > dl_size(dl)) {
        return 0; /* out of range */
    }
    curr = dl->head;
    nitem--;
    i=0;
    while(curr != 0)  {
        if(i == nitem)
            return curr;
        curr = curr->__next__;
        i++;
    }
    return 0; /* not found */
}


/***************************************************************
 *      dl_find - find backward
 ***************************************************************/
void * dl_find_back(dl_list_t *dl, void *item)
{
    register dl_item_t * curr;

    curr = dl->tail;

    while(curr != 0)  {
        if(curr==item)
            return item;
        curr = curr->__prev__;
    }
    return (void *)0; /* not found */
}

/***************************************************************
 *      Seek first item
 ***************************************************************/
void * dl_first(dl_list_t *dl)
{
    return dl->head;
}

/***************************************************************
 *      Seek last item
 ***************************************************************/
void * dl_last(dl_list_t *dl)
{
    return dl->tail;
}

/***************************************************************
 *      next Item
 ***************************************************************/
void * dl_next(void *curr)
{
    if(curr)
        return ((dl_item_t *)curr)->__next__;
    return (void *)0;
}

/***************************************************************
 *      previous Item
 ***************************************************************/
void * dl_prev(void *curr)
{
    if(curr)
        return ((dl_item_t *)curr)->__prev__;
    return (void *)0;
}

/***************************************************************
 *
 ***************************************************************/
size_t dl_id(void *item)
{
    if(item)
        return ((dl_item_t *)item)->__id__;
    return 0;
}

/***************************************************************
 *  Check if a new item has links: MUST have NO links
 *  Return TRUE if it has links
 ***************************************************************/
PRIVATE BOOL check_links(register dl_item_t *item)
{
    if(item->__prev__ || item->__next__ || item->__dl__) {
        // print my errors please
        print_error(
            PEF_ABORT,
            "YUNETA ERROR",
            "check_links(): Wrong dl_item_t, WITH links."
            " %s %s %d",
            get_host_name(),
            get_process_name(),
            get_pid()
        );
        return TRUE;
    }
    return FALSE;
}

/***************************************************************
 *  Check if a item has no links: MUST have links
 *  Return TRUE if it has not links
 ***************************************************************/
PRIVATE BOOL check_no_links(register dl_item_t *item)
{
    if(!item->__dl__) {
        print_error(
            PEF_ABORT,
            "YUNETA ERROR",
            "check_no_links(): Wrong dl_item_t, WITHOUT links."
            " %s %s %d",
            get_host_name(),
            get_process_name(),
            get_pid()
        );
        return TRUE;
    }
    return FALSE;
}

/***************************************************************
 *      Add at end of the list
 ***************************************************************/
int dl_add(dl_list_t *dl, void *item)
{
    if(check_links(item)) return -1;

    if(dl->tail==0) { /*---- Empty List -----*/
        ((dl_item_t *)item)->__prev__=0;
        ((dl_item_t *)item)->__next__=0;
        dl->head = item;
        dl->tail = item;
    } else { /* LAST ITEM */
        ((dl_item_t *)item)->__prev__ = dl->tail;
        ((dl_item_t *)item)->__next__ = 0;
        dl->tail->__next__ = item;
        dl->tail = item;
    }
    dl->__itemsInContainer__++;
    dl->__last_id__++;
    ((dl_item_t *)item)->__id__ = dl->__last_id__;
    ((dl_item_t *)item)->__dl__ = dl;

    return 0;
}

/***************************************************************
 *   Insert at the head
 ***************************************************************/
int dl_insert(dl_list_t *dl, void *item)
{
    if(check_links(item)) return -1;

    if(dl->head==0) { /*---- Empty List -----*/
        ((dl_item_t *)item)->__prev__=0;
        ((dl_item_t *)item)->__next__=0;
        dl->head = item;
        dl->tail = item;
    } else  { /* FIRST ITEM */
        ((dl_item_t *)item)->__prev__ = 0;
        ((dl_item_t *)item)->__next__ = dl->head;
        dl->head->__prev__ = item;
        dl->head = item;
    }

    dl->__itemsInContainer__++;
    dl->__last_id__++;
    ((dl_item_t *)item)->__id__ = dl->__last_id__;
    ((dl_item_t *)item)->__dl__ = dl;

    return 0;
}

/***************************************************************
 *   Insert a new item before current item.
 ***************************************************************/
int dl_insert_before(dl_list_t *dl, void *curr, void *new_item)
{
    if(check_links(new_item)) return -1;

    if(dl->head==0) { /*---- Empty List -----*/
        ((dl_item_t *)new_item)->__prev__=0;
        ((dl_item_t *)new_item)->__next__=0;
        dl->head = new_item;
        dl->tail = new_item;
    } else if(curr==0 || ((dl_item_t *)curr)->__prev__==0) { /* FIRST ITEM. (curr==dl->head) */
        ((dl_item_t *)new_item)->__prev__ = 0;
        ((dl_item_t *)new_item)->__next__ = dl->head;
        dl->head->__prev__ = new_item;
        dl->head = new_item;
    } else { /* MIDDLE OR LAST ITEM */
        ((dl_item_t *)curr)->__prev__->__next__=new_item;
        ((dl_item_t *)new_item)->__next__ = curr;
        ((dl_item_t *)new_item)->__prev__ = ((dl_item_t *)curr)->__prev__;
        ((dl_item_t *)curr)->__prev__=new_item;
    }

    dl->__itemsInContainer__++;
    dl->__last_id__++;
    ((dl_item_t *)new_item)->__id__ = dl->__last_id__;
    ((dl_item_t *)new_item)->__dl__ = dl;

    return 0;
}

/***************************************************************
 *   Insert a new item after current item.
 ***************************************************************/
int dl_insert_after(dl_list_t *dl, void *curr, void *new_item)
{
    if(check_links(new_item)) return -1;

    if(dl->head==0) { /*---- Empty List -----*/
        ((dl_item_t *)new_item)->__prev__=0;
        ((dl_item_t *)new_item)->__next__=0;
        dl->head = new_item;
        dl->tail = new_item;
    } else if(curr==0 || ((dl_item_t *)curr)->__next__==0) { /* LAST ITEM */
        ((dl_item_t *)new_item)->__prev__ = dl->tail;
        ((dl_item_t *)new_item)->__next__ = 0;
        dl->tail->__next__ = new_item;
        dl->tail = new_item;
    } else {
        ((dl_item_t *)curr)->__next__->__prev__=new_item;
        ((dl_item_t *)new_item)->__next__ = ((dl_item_t *)curr)->__next__;
        ((dl_item_t *)new_item)->__prev__ = curr;
        ((dl_item_t *)curr)->__next__=new_item;
    }

    dl->__itemsInContainer__++;
    dl->__last_id__++;
    ((dl_item_t *)new_item)->__id__ = dl->__last_id__;
    ((dl_item_t *)new_item)->__dl__ = dl;

    return 0;
}

/***************************************************************
 *    Delete current item
 ***************************************************************/
int dl_delete(dl_list_t *dl, void * curr_, void (*fnfree)(void *))
{
    register dl_item_t * curr = curr_;
    /*-------------------------*
     *     Check nulls
     *-------------------------*/
    if(curr==0) {
        return -1;
    }
    if(check_no_links(curr))
        return -1;

    if(curr->__dl__ != dl) {
        print_error(
            PEF_ABORT,
            "YUNETA ERROR",
            "dl_delete(): Deleting item with DIFFERENT dl_list_t."
            " %s %s %d",
            get_host_name(),
            get_process_name(),
            get_pid()
        );
        return -1;
    }

    /*-------------------------*
     *     Check list
     *-------------------------*/
    if(dl->head==0 || dl->tail==0 || dl->__itemsInContainer__==0) {
        print_error(
            PEF_ABORT,
            "YUNETA ERROR",
            "dl_delete(): Deleting item in EMPTY list."
            " %s %s %d",
            get_host_name(),
            get_process_name(),
            get_pid()
        );
        return -1;
    }

    /*------------------------------------------------*
     *                 Delete
     *------------------------------------------------*/
    if(((dl_item_t *)curr)->__prev__==0) {
        /*------------------------------------*
         *  FIRST ITEM. (curr==dl->head)
         *------------------------------------*/
        dl->head = dl->head->__next__;
        if(dl->head) /* is last item? */
            dl->head->__prev__=0; /* no */
        else
            dl->tail=0; /* yes */

    } else {
        /*------------------------------------*
         *    MIDDLE or LAST ITEM
         *------------------------------------*/
        ((dl_item_t *)curr)->__prev__->__next__ = ((dl_item_t *)curr)->__next__;
        if(((dl_item_t *)curr)->__next__) /* last? */
            ((dl_item_t *)curr)->__next__->__prev__ = ((dl_item_t *)curr)->__prev__; /* no */
        else
            dl->tail= ((dl_item_t *)curr)->__prev__; /* yes */
    }

    /*-----------------------------*
     *  Decrement items counter
     *-----------------------------*/
    dl->__itemsInContainer__--;

    /*-----------------------------*
     *  Reset pointers
     *-----------------------------*/
    ((dl_item_t *)curr)->__prev__ = 0;
    ((dl_item_t *)curr)->__next__ = 0;
    ((dl_item_t *)curr)->__dl__ = 0;

    /*-----------------------------*
     *  Free item
     *-----------------------------*/
    if(fnfree)
        (*fnfree)(curr);

    return 0;
}

/***************************************************************
 *    Delete current item
 ***************************************************************/
int dl_delete_item(void * curr_, void (*fnfree)(void *))
{
    register dl_item_t * curr = curr_;
    /*-------------------------*
     *     Check nulls
     *-------------------------*/
    if(curr==0) {
        return -1;
    }
    if(check_no_links(curr)) return -1;

    return dl_delete(curr->__dl__, curr, fnfree);
}

/***************************************************************
 *    Return list position of item (relative to 1)
 *    Return 0 if not found
 ***************************************************************/
size_t dl_index(dl_list_t *dl, void *item)
{
    register dl_item_t * curr;
    register size_t i;

    curr = dl->head;

    i=0;
    while(curr != 0)  {
        if(curr==item)
            return i+1;
        curr = curr->__next__;
        i++;
    }
    return 0; /* not found */
}

/***************************************************************
 *   Return number of items in list
 ***************************************************************/
size_t dl_size(dl_list_t *dl)
{
    if(!dl) {
        return 0;
    }
    return dl->__itemsInContainer__;
}

/***************************************************************
 *   Return the dl_list_t of a item
 ***************************************************************/
dl_list_t *dl_dl_list(void *item_)
{
    dl_item_t *item = item_;
    return item->__dl__;
}

/***************************************************************
 *  Move the source list to another destination list
 *  and reset the source
 ***************************************************************/
void dl_move(dl_list_t *destination, dl_list_t *source)
{
    destination->head = source->head;
    destination->tail = source->tail;
    destination->__itemsInContainer__ = source->__itemsInContainer__;
    dl_init(source);
}
