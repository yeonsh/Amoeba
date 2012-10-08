/*	@(#)log10.c	1.1	91/04/09 09:24:19 */
/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */

#include	<math.h>
#include	<errno.h>
#include	"localmath.h"

double
log10(x)
double x;
{
	if (__IsNan(x)) {
		errno = EDOM;
		return x;
	}
	if (x < 0) {
		errno = EDOM;
		return -HUGE_VAL;
	}
	else if (x == 0) {
		errno = ERANGE;
		return -HUGE_VAL;
	}

	return log(x) / M_LN10;
}
