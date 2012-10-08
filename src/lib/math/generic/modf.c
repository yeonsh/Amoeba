/*	@(#)modf.c	1.2	94/04/07 10:57:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <float.h>
#include <errno.h>
#include <math.h>
#include "localmath.h"

double
modf(x, py)
  	double	x, *py;
{
	int exp;
	long l;
	int neg = x < 0;

	if (__IsNan(x)) {
		errno = EDOM;
		*py = x;
		return x;
	}
	if (x > DBL_MAX || x < -DBL_MAX) {
		*py = x;
		return 0.0;
	}

	(void) frexp(x, &exp);
	if (exp <= 0) {
		*py = 0.0;
		return x;
	}
	if (exp >= DBL_MANT_DIG) {
		*py = x;
		return 0.0;
	}
	if (neg) {
		x = -x;
	}
	if (exp <= 31) {
		l = x;
		*py = l;
		x -= *py;
	}
	else {
		l = x/(1L << 30);
		*py = l*(double)(1L << 30);
		x -= *py;
		l = x;
		*py += l;
		x -= l;
	}
	if (neg) *py = -*py;
	return neg ? -x : x;
}
