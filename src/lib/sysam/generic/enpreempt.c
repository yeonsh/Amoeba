/*	@(#)enpreempt.c	1.3	96/02/27 11:21:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "thread.h"

/* the real system call */
extern void _thread_en_preempt _ARGS((struct thread_data **));

extern struct thread_data * _thread_local;

void
thread_enable_preemption()
{
    _thread_en_preempt(&_thread_local);
}
