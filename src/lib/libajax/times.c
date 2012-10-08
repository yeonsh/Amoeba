/*	@(#)times.c	1.4	94/04/07 09:54:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix-compliant times() function */

#include <sys/types.h>
#include <sys/times.h>
#include <limits.h>
#include <module/syscall.h>

clock_t
times(tp)
	register struct tms *tp;
{
	clock_t t;

	/* This is not really supported, but should be there so programs
	   that use it will still link. */
	tp->tms_utime = 0;
	tp->tms_stime = 0;
	tp->tms_cutime = 0;
	tp->tms_cstime = 0;

	/* The Amoeba CLK_TCK is 1000, so we need no rescaling */
	t =  sys_milli();
	if (t < 0)	/* Keep the result positive, just to be safe */
	    t += LONG_MAX;
	return t;
}
