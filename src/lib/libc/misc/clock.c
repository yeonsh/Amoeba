/*	@(#)clock.c	1.2	94/04/07 10:47:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <time.h>

clock_t
clock()
{
	return (clock_t) -1;	/* cpu time used is not available */
}
