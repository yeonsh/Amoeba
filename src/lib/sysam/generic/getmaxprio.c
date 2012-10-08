/*	@(#)getmaxprio.c	1.3	96/02/27 11:22:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "thread.h"

/* The system call proper: */
extern void _thread_get_max_prio _ARGS((long *));

void
thread_get_max_priority(max)
long *max;
{
    _thread_get_max_prio(max);
}
