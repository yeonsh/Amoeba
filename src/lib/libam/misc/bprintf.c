/*	@(#)bprintf.c	1.4	96/02/27 11:01:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "module/strmisc.h"

#ifdef __STDC__
#include "stdarg.h"
#else
#include "varargs.h"
#endif

/* This is not very pretty and must be done right some day */

#ifdef __STDC__
char *bprintf(char *begin, char *end, char *fmt, ...)
#else
/*VARARGS3*/
char *
bprintf(begin, end, fmt, va_alist)
char *	begin;
char *	end;
char *	fmt;
va_dcl
#endif
{
	char	buf[1024];
	char	*p = buf;
	va_list args;

#ifdef __STDC__
	va_start(args, fmt);
#else
	va_start(args);
#endif
	(void) vsprintf(buf, fmt, args);
	va_end(args);
	if (begin == (char *) 0 || end == (char *) 0) {
		begin = end = (char *) 0;
		printf("%s", buf);
	} else {
		while (*p != '\0' && begin < end)
			*begin++ = *p++;
		if (begin < end)
		    *begin = 0;
	}
	return begin;
}
