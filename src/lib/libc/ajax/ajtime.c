/*	@(#)ajtime.c	1.3	94/04/07 10:24:01 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ajax.h"

errstat
_ajax_time(pt)
	long *pt;
{
	long seconds;
	int millisecs, tz, dst;
	errstat err;
	
	if ((err = tod_gettime(&seconds, &millisecs, &tz, &dst)) == STD_OK)
		*pt = seconds;
	else
		*pt = 0;
	return err;
}
