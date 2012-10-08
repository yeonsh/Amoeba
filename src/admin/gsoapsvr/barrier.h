/*	@(#)barrier.h	1.1	96/02/27 10:02:35 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#ifndef BARRIER_H
#define BARRIER_H

#ifndef barrier_type
#define barrier_type long
#endif

struct barlist {
    barrier_type    bl_wait;
    semaphore	    bl_sema;
    struct barlist *bl_next;
};

struct barrier {
    barrier_type    bar_value;
    mutex	    bar_mutex;
    struct barlist *bar_waiting;
};

struct barrier *bar_create   _ARGS((barrier_type initial));
void		bar_wait_for _ARGS((struct barrier *bar, barrier_type val));
void		bar_wake_up  _ARGS((struct barrier *bar, barrier_type val));

#endif /* BARRIER_H */
