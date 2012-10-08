/*	@(#)gettimeofday.c	1.2	94/04/07 09:47:28 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* gettimeofday(2) system call emulation */

#include "ajax.h"
#include <sys/time.h>

int
gettimeofday(tp, tzp)
	struct timeval *tp;
	struct timezone *tzp;
{
	long seconds;
	int millisecs, tz, dst;
	int err;
	
	if ((err = tod_gettime(&seconds, &millisecs, &tz, &dst)) != 0) {
		/* Return fake values anyway */
		seconds = 0;
		millisecs = 0;
		tz = 0;
		dst = 0;
	}
	
	if (tp != NULL) {
		tp->tv_sec = seconds;
		tp->tv_usec = millisecs*1000;
	}
	if (tzp != NULL) {
		tzp->tz_minuteswest = tz;
		tzp->tz_dsttime = dst;
	}
	
	if (err != 0)
		ERR(EIO, "gettimeofday: tod_gettime failed");
	return 0;
}
