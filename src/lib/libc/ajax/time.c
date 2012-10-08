/*	@(#)time.c	1.3	94/04/07 10:33:13 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* ANSI C and Posix-compatible time() function */

#include "ajax.h"
#include <time.h>

time_t
time(tp)
	time_t *tp;
{
	long seconds;
	int millisecs, tz, dst;
	
	if (tod_gettime(&seconds, &millisecs, &tz, &dst) != STD_OK) {
		if (tp != NULL)
			*tp = 0;
		ERR(EIO, "time: tod_gettime failed");
	}
	if (tp != NULL)
		*tp = seconds;
	return seconds;
}
