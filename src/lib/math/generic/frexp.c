/*	@(#)frexp.c	1.2	94/04/07 10:57:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef NOFLOAT

#include <math.h>
#include <float.h>
#include <errno.h>
#include "localmath.h"

/*
** Compute mantissa ('fraction') and exponent of a double precision value.
** Author: Guido van Rossum, CWI.
*/

double
frexp(x, eptr)
	double x;
	int *eptr;
{
	int exp = 0;
	int sign = 1;
	
	if (__IsNan(x)) {
                errno = EDOM;
		*eptr = DBL_MAX_EXP;
		return(x);
	}
	if (x > DBL_MAX || x < -DBL_MAX) {
                errno = EDOM;
		*eptr = DBL_MAX_EXP;
		if ( x < 0 ) return -0.5;
		else return 0.5;
	}

	if (x < 0.0) {
		x = -x;
		sign = -1;
	}
	
	if (x != 0.0) {
		while (x >= (double) (1L << 30)) {
			exp += 30;
			x /= (double) (1L << 30);
		}
		while (x < 1.0 / (double) (1L << 30)) {
			exp -= 30;
			x *= (double) (1L << 30);
		}
		while (x < 0.5) {
			exp--;
			x *= 2.0;
		}
		while (x >= 1.0) {
			exp++;
			x /= 2.0;
		}
	}
	
	*eptr = exp;
	return x * sign;
}

#else /* NOFLOAT */

#ifndef lint
/* to keep ranlib and nm happy */
static int x;
#endif

#endif
