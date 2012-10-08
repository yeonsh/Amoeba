/*	@(#)fmod.c	1.1	91/04/09 09:22:24 */
/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Hans van Eck
 */

#include	<math.h>
#include	<errno.h>

double
fmod(x, y)
double x, y;
{
	/* long	i; */
	double val;
	double frac;

	if (y == 0) {
		errno = EDOM;
		return 0;
	}
	frac = modf( x / y, &val);

	return frac * y;

/*
	val = x / y;
	if (val > LONG_MIN && val < LONG_MAX) {
		i = val;
		return x - i * y;
	}
*/
}
