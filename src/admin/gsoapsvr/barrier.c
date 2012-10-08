/*	@(#)barrier.c	1.1	96/02/27 10:02:34 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <limits.h>
#include "amoeba.h"
#include "semaphore.h"
#include "module/mutex.h"
#include "monitor.h"

#include "diag.h"
#include "barrier.h"

/*
 * Barrier implementation.
 */

static struct barlist *Bar_freelist = NULL;

static struct barlist *
new_barlist(val)
barrier_type val;
{
    struct barlist *bl;

    if ((bl = Bar_freelist) != NULL) {
	Bar_freelist = Bar_freelist->bl_next;
    } else {
	MON_EVENT("barrier: alloc node");
	if ((bl = (struct barlist *) malloc(sizeof(struct barlist))) == NULL) {
	    panic("barrier: cannot allocate new barrier node");
	}
    }

    bl->bl_wait = val;
    sema_init(&bl->bl_sema, 0);
    bl->bl_next = NULL;
    return bl;
}

void
bar_wait_for(bar, val)
struct barrier *bar;
barrier_type	val;
{
    /* Block until the barrier has reached value val. */
    struct barlist *bl = NULL;

    mu_lock(&bar->bar_mutex);
    if (bar->bar_value < val) {
	/* Add it to the waiting list */
	bl = new_barlist(val);
	bl->bl_next = bar->bar_waiting;
	bar->bar_waiting = bl;
    }
    mu_unlock(&bar->bar_mutex);

    if (bl != NULL) {
	struct barlist **pbl;

	/* Block until the barrier has increased enough */
	MON_EVENT("barrier: block");
	sema_down(&bl->bl_sema);

	/* Now that we run again, remove the barrier node */
	mu_lock(&bar->bar_mutex);
	for (pbl = &bar->bar_waiting; *pbl != bl; pbl = &(*pbl)->bl_next) {
	    /* skip */
	}
	*pbl = bl->bl_next;
	bl->bl_next = Bar_freelist;
	Bar_freelist = bl;
	mu_unlock(&bar->bar_mutex);
    }
}

void
bar_wake_up(bar, val)
struct barrier *bar;
barrier_type    val;
{
    struct barlist *bl;

    /* The barrier is increased to value val.
     * See if a thread waiting for it can be woken up.
     */
    mu_lock(&bar->bar_mutex);
    bar->bar_value = val;

    for (bl = bar->bar_waiting; bl != NULL; bl = bl->bl_next) {
	if (val >= bl->bl_wait) {
	    bl->bl_wait = LONG_MAX; 	/* avoid further wakeups */
	    MON_EVENT("barrier: unblock");
	    sema_up(&bl->bl_sema);
	}
    }
    mu_unlock(&bar->bar_mutex);
}

struct barrier *
bar_create(val)
barrier_type val;
{
    struct barrier *bar = (struct barrier *) malloc(sizeof(struct barrier));

    if (bar == NULL) {
	panic("barrier: cannot allocate barrier");
    }

    mu_init(&bar->bar_mutex);
    bar->bar_waiting = NULL;
    bar->bar_value = val;
    return bar;
}
