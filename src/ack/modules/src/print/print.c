/*	@(#)print.c	1.2	93/01/15 17:14:37 */
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
/*FORMAT0v $
	%s = char *
	%l = long
	%c = int
	%[uxbo] = unsigned int
	%d = int
$ */
#ifdef __STDC__
print(char *fmt, ...)
#else
print(fmt, va_alist) char *fmt; va_dcl
#endif
{
	va_list args;
	char buf[SSIZE];

	va_dostart(args, fmt);
	sys_write(STDOUT, buf, _format(buf, fmt, args));
	va_end(args);
}
