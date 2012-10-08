/*	@(#)fprintf.c	1.2	94/04/07 10:52:38 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fprintf - write output on a stream
 */

#include <stdio.h>
#include "loc_incl.h"

/*VARARGS2*/
#ifdef __STDC__
int fprintf(FILE *stream, const char *format, ...)
#else
int fprintf(stream, format, va_alist) FILE *stream; char *format; va_dcl
#endif
{
	va_list ap;
	int retval;
	
#ifdef __STDC__
	va_start(ap, format);
#else
        va_start(ap);
#endif
	retval = _doprnt (format, ap, stream);

	va_end(ap);

	return retval;
}
