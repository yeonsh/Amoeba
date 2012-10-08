/*	@(#)isnan.c	1.1	91/04/09 09:23:17 */
/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * __IsNan(fl) returns 1 if fl is "not a number", otherwise 0.
 */

int
__IsNan(d)
double d;
{
#ifdef IEEE_FP
	float f = d;

	if ((*((long *) &f) & 0x7f800000) == 0x7f800000 &&
	    (*((long *) &f) & 0x007fffff) != 0) return 1;
#endif
	return 0;
}
