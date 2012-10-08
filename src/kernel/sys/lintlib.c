/*	@(#)lintlib.c	1.3	94/04/06 10:06:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Routines in assembler in this directory
 */
#include "amoeba.h"
#include "global.h"
#include "machdep.h"
#include "exception.h"
#include "kthread.h"

/*ARGSUSED*/
void
switchthread(tp)
struct thread *tp;
{
#ifndef NDEBUG
    checkstack();
#endif
}

/*ARGSUSED*/
int
forkthread(tp)
struct thread *tp;
{
    return 0;
}
