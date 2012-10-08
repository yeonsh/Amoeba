/*	@(#)printf.c	1.2	94/04/07 10:54:32 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * printf - write on the standard output stream
 */

#include <stdio.h>
#include "loc_incl.h"

/*VARARGS1*/
#ifdef __STDC__
int printf(const char *format, ...)
#else
int printf(format, va_alist) char *format; va_dcl
#endif
{
	va_list ap;
	int retval;

#ifdef __STDC__
	va_start(ap, format);
#else
        va_start(ap);
#endif
	retval = _doprnt(format, ap, stdout);

	va_end(ap);

	return retval;
}
