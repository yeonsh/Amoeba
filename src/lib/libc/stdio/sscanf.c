/*	@(#)sscanf.c	1.2	94/04/07 10:55:39 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * sscanf - read formatted output from a string
 */

#include <stdio.h>
#include <string.h>
#include "loc_incl.h"
#include "locking.h"

/*VARARGS2*/
#ifdef __STDC__
int sscanf(const char *s, const char *format, ...)
#else
int sscanf(s, format, va_alist) const char *s; const char *format; va_dcl
#endif
{
	va_list ap;
	int retval;
	FILE tmp_stream;

#ifdef __STDC__
	va_start(ap, format);
#else
	va_start(ap);
#endif

	tmp_stream._fd     = -1;
	tmp_stream._flags  = _IOREAD + _IONBF + _IOREADING;
	tmp_stream._buf    = (unsigned char *) s;
	tmp_stream._ptr    = (unsigned char *) s;
	tmp_stream._count  = strlen(s);
	mu_init(&tmp_stream._mu);

	retval = _doscan(&tmp_stream, format, ap);

	va_end(ap);

	return retval;
}
