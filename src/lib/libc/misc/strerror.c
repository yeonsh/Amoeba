/*	@(#)strerror.c	1.2	94/04/07 10:49:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <errlist.h>

char *
strerror(errnum)
register int errnum;
{
	if (errnum < 0 || errnum >= sys_nerr)
		return "unknown error";
	return (char *)sys_errlist[errnum];
}

