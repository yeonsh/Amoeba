/*	@(#)vprintf.c	1.4	96/02/27 11:12:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * vprintf - formatted output without ellipsis to the standard output stream
 */

#include <stdio.h>
#include "loc_incl.h"

#ifdef __STDC__
int vprintf(const char *format, va_list arg)
#else
int vprintf(format, arg) char *format; va_list arg;
#endif
{
	return _doprnt(format, arg, stdout);
}
