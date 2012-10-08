/*	@(#)slist.c	1.3	94/04/07 14:53:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "alloc.h"
#include "slist.h"

PUBLIC void
DoAppend(list, item)
struct slist **list;
generic item;
{
    register struct cons *cons;
    register struct slist *header;

    (cons = new_cons())->co_item = item;
    assert(list != NULL); /* check if header is legal */
    if (IsEmpty(header = *list)) {
        *list = header = new_slist();
        header->sl_first = header->sl_last = cons;
    } else {
	cons->co_prev = header->sl_last;
        header->sl_last = header->sl_last->co_next = cons;
    }
}

PUBLIC void
Insert(list, before, item)
struct slist **list;
struct cons *before;
generic item;
{
    register struct cons *cons;

    assert(list != NULL); /* check if header is legal */
    assert(before != NULL);
    assert(!IsEmpty(*list));

    (cons = new_cons())->co_item = item;

    cons->co_next = before;
    if ((cons->co_prev = before->co_prev) == NULL) {
	/* Insert before first one */
	(*list)->sl_first = cons;
    } else {
	before->co_prev->co_next = cons;
    }
    before->co_prev = cons;
}

PUBLIC void
AppendList(listp, list)
struct slist **listp, *list;
{
    register struct cons *cons;

    for (cons = Head(list); cons != NULL; cons = Next(cons)) {
	Append(listp, Item(cons));
    }
}

PUBLIC void
Iterate(list, fun)
struct slist *list;
void (*fun) P((void *));
{
    register struct cons *cons;
    
    if (!IsEmpty(list)) {
	for (cons = HeadOf(list); cons != NULL; cons = Next(cons)) {
	    (*fun)(Item(cons));
	}
    }
}

PUBLIC void
Remove(list, cons)
struct slist **list;
struct cons *cons;
/* Remove the element pointed to by cons from list */
{
/* cases:  (1)`Del' (2)`Del El+' (3)`El+ Del' (4)`El+ Del El+' */

    if (cons->co_prev != NULL) { /* 3 | 4 */
	cons->co_prev->co_next = cons->co_next;
    } else { /* 1 | 2 */
	(*list)->sl_first = cons->co_next; /* 2 */
    }

    if (cons->co_next != NULL) { /* 2 | 4 */
	cons->co_next->co_prev = cons->co_prev;
    } else { /* 1 | 3 */
	(*list)->sl_last = cons->co_prev; /* 3 */
    }

    free_cons(cons);
    if ((*list)->sl_first == NULL) { /* 1 */
	free_slist(*list);
	*list = NULL;
    }
}

PUBLIC struct slist *
DestructiveAppend(left, right)
struct slist *left, *right;
{
    if (IsEmpty(left)) {
	return(right);
    } else if (IsEmpty(right)) {
	return(left);
    } else {
	left->sl_last->co_next = HeadOf(right);
	right->sl_first->co_prev = TailOf(left);
	left->sl_last = right->sl_last;
	free_slist(right);
	return(left);
    }
}

PUBLIC void
Splice(list, cons, insert)
struct slist **list;
struct cons *cons;
struct slist *insert;
{
    if (IsEmpty(insert)) {
	Remove(list, cons);
    } else {
	if (cons->co_next != NULL) {
	    cons->co_next->co_prev = TailOf(insert);
	    TailOf(insert)->co_next = cons->co_next;
	} else {
	    (*list)->sl_last = TailOf(insert);
	}
	if (cons->co_prev != NULL) {
	    cons->co_prev->co_next = HeadOf(insert);
	    HeadOf(insert)->co_prev = cons->co_prev;
	} else {
	    (*list)->sl_first = HeadOf(insert);
	}
	free_cons(cons);
    }
}

PUBLIC int
Length(list)
struct slist *list;
{
    register int length = 0;
    register struct cons *cons;

    for (cons = Head(list); cons != NULL; cons = Next(cons)) {
	length++;
    }
    return(length);
}

PUBLIC generic
DoRemoveHead(listp)
struct slist **listp;
{
    register generic g;
    register struct cons *head;

    assert(!IsEmpty(*listp));
    g = Item(head = HeadOf(*listp));
    Remove(listp, head);
    return(g);
}
