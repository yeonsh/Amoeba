/*	@(#)ftime.c	1.2	94/04/07 10:28:05 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* v7-compatible ftime(3) function */

#include "ajax.h"
#include "module/tod.h"

#include <sys/types.h>
#include <sys/timeb.h>

int
ftime(tp)
	register struct timeb *tp;
{
	long seconds;
	int millisecs, tz, dst;
	
	if (tod_gettime(&seconds, &millisecs, &tz, &dst) != STD_OK) {
		/* Set fake values -- callers don't expect this to fail */
		tp->time = 0;
		tp->millitm = 0;
		tp->timezone = 0;
		tp->dstflag = 0;
		ERR(EIO, "ftime: tod_gettime failed");
	}
	else {
		tp->time = seconds;
		tp->millitm = millisecs;
		tp->timezone = tz;
		tp->dstflag = dst;
	}
	return 0;
}
