/*	@(#)nice.c	1.2	94/04/07 09:49:30 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* nice(3) library call */
/* TO DO: check for errno after getpriority call */

#include <sys/time.h>
#include <sys/resource.h>

int
nice(incr)
	int incr;
{
	return setpriority(PRIO_PROCESS, 0,
		getpriority(PRIO_PROCESS, 0) + incr);
}
