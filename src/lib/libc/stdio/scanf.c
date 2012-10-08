/*	@(#)scanf.c	1.2	94/04/07 10:55:16 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * scanf.c - read formatted input from the standard input stream
 */

#include <stdio.h>
#include "loc_incl.h"

#ifdef __STDC__
int scanf(const char *format, ...)
#else
int scanf(format, va_alist) char *format; va_dcl
#endif
{
	va_list ap;
	int retval;

#ifdef __STDC__
	va_start(ap, format);
#else
	va_start(ap);
#endif
	retval = _doscan(stdin, format, ap);

	va_end(ap);

	return retval;
}


