/*	@(#)fscanf.c	1.2	94/04/07 10:53:10 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fscanf.c - read formatted input from stream
 */

#include <stdio.h>
#include "loc_incl.h"

/*VARARGS2*/
#ifdef __STDC__
int fscanf(FILE *stream, const char *format, ...)
#else
int fscanf(stream, format, va_alist) FILE *stream; char *format; va_dcl
#endif
{
	va_list ap;
	int retval;

#ifdef __STDC__
	va_start(ap, format);
#else
	va_start(ap);
#endif
	retval = _doscan(stream, format, ap);

	va_end(ap);

	return retval;
}
