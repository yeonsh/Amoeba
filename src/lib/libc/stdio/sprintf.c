/*	@(#)sprintf.c	1.2	94/04/07 10:55:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * sprintf - print formatted output on an array
 */

#include <stdio.h>
#include "loc_incl.h"
#include "locking.h"


/*VARARGS2*/
#ifdef __STDC__
int sprintf(char *s, const char *format, ...)
#else
int sprintf(s, format, va_alist) char *s, *format; va_dcl
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
	tmp_stream._flags  = _IOWRITE + _IONBF + _IOWRITING;
	tmp_stream._buf    = (unsigned char *) s;
	tmp_stream._ptr    = (unsigned char *) s;
	tmp_stream._count  = 32767;
	mu_init(&tmp_stream._mu);

	retval = _doprnt(format, ap, &tmp_stream);
	putc('\0',&tmp_stream);

	va_end(ap);

	return retval;
}
