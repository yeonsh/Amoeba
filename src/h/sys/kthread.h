/*	@(#)kthread.h	1.5	96/02/27 10:39:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __KTHREAD_H__
#define __KTHREAD_H__

extern struct thread {

#include "mpx.H"
#include "mpx_md.H"
#ifndef NO_OLDRPC
#include "trans.H"			/* trans module */
#include "portcache.H"			/* portcache module */
#endif /* NO_OLDRPC */
	char *tk_aux;			/* auxiliary pointer */
	/* really a hack to make process thread more efficient */
	int   tk_slotno;		/* number of thread */
} *thread, *curthread, *upperthread;

extern uint16 totthread;

#define NILTHREAD		((struct thread *) 0)

#define THREADSLOT(t)		((t)->tk_slotno)

typedef struct thread 		thread_t;


void switchthread _ARGS((struct thread * tp));
int forkthread _ARGS((struct thread * tp));

#endif /* __KTHREAD_H__ */
