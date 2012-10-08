/*	@(#)_ftou.c	1.1	94/04/07 10:44:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define  _POSIX_SOURCE
#include <limits.h>

unsigned int
_ftou(fl)
long fl;
/* Float to unsigned conversion routine called by the Sun compiler.
 * Note: the argument is really a 4-bytes float, but specifying 'float'
 * will let the compiler assume it is passed as 'double' (according to
 * the default argument promotions).
 */
{
    float f = *(float *)&fl;

    if (f >= (float) LONG_MAX) {
	/* have to be careful in this case */
	if (f >= (float) ULONG_MAX) {
	    return ULONG_MAX;
	} else {
	    return (unsigned)LONG_MAX + (unsigned) (int)(f - (float)LONG_MAX);
	}
    } else {
	/* default float->int conversion should work */
	return (unsigned int) (int) f;
    }
}
