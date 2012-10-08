/*	@(#)floor.c	1.1	91/04/09 09:22:16 */
/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */

#include	<math.h>

double
floor(x)
double x;
{
	double val;

	return modf(x, &val) < 0 ? val - 1.0 : val ;
	/*	this also works if modf always returns a positive
		fractional part
	*/
}
