/*	@(#)_dtou.c	1.3	96/02/29 16:47:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define  _POSIX_SOURCE
#include <limits.h>

/*
 * compilers always call this routine as __dtou, independently of their
 * tendencies to prepend underscores to function names.  If the compiler
 * doesn't prepend then we'd better do it.
 */
#ifdef NO_UNDERSCORE
#define	DTOU	__dtou
#else
#define	DTOU	_dtou
#endif

unsigned int
DTOU(d)
double d;
{
    if (d >= (double) LONG_MAX) {
	/* have to be careful in this case */
	if (d >= (double) ULONG_MAX) {
	    return ULONG_MAX;
	} else {
	    return (unsigned)LONG_MAX + (unsigned) (int)(d - (double)LONG_MAX);
	}
    } else {
	/* default double->int conversion should work */
	return (unsigned int) (int) d;
    }
}
