/*	@(#)sprint.c	1.2	93/01/15 17:14:44 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#ifdef __STDC__
#include <stdarg.h>
#define va_dostart(ap, prm)     va_start(ap, prm)
#else
#include <varargs.h>
#define va_dostart(ap, prm)     va_start(ap)
#endif
#include <system.h>
#include "param.h"

/*VARARGS2*/
/*FORMAT1v $
	%s = char *
	%l = long
	%c = int
	%[uxbo] = unsigned int
	%d = int
$ */
#ifdef __STDC__
char *sprint(char *buf, char *fmt, ...)
#else
char *sprint(buf, fmt, va_alist) char *buf; char *fmt; va_dcl
#endif
{
	va_list args;

	va_dostart(args, fmt);
	buf[_format(buf, fmt, args)] = '\0';
	va_end(args);
	return buf;
}
