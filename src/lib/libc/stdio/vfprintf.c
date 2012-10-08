/*	@(#)vfprintf.c	1.4	96/02/27 11:12:13 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * vfprintf - formatted output without ellipsis
 */

#include <stdio.h>
#include "loc_incl.h"

#ifdef __STDC__
int vfprintf(FILE *stream, const char *format, va_list arg)
#else
int vfprintf(stream, format, arg) FILE *stream; char *format; va_list arg;
#endif
{
	return _doprnt (format, arg, stream);
}
