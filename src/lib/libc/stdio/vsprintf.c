/*	@(#)vsprintf.c	1.4	96/02/27 11:12:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * vsprintf - print formatted output without ellipsis on an array
 */

#include <stdio.h>
#include "loc_incl.h"
#include "locking.h"

#ifdef __STDC__
int vsprintf(char *s, const char *format, va_list arg)
#else
int vsprintf(s, format, arg) char *s; char *format; va_list arg;
#endif
{
	int retval;
	FILE tmp_stream;

	tmp_stream._fd     = -1;
	tmp_stream._flags  = _IOWRITE + _IONBF + _IOWRITING;
	tmp_stream._buf    = (unsigned char *) s;
	tmp_stream._ptr    = (unsigned char *) s;
	tmp_stream._count  = 32767;
	mu_init(&tmp_stream._mu);

	retval = _doprnt(format, arg, &tmp_stream);
	putc('\0',&tmp_stream);

	return retval;
}
