/*	@(#)atoi.c	1.2	92/05/13 15:01:29 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include	<ctype.h>

/* We do not use strtol here for backwards compatibility in behaviour on
   overflow.
*/
int
atoi(nptr)
register char *nptr;
{
	int total = 0;
	int minus = 0;

	while (isspace(*nptr)) nptr++;
	if (*nptr == '+') nptr++;
	else if (*nptr == '-') {
		minus = 1;
		nptr++;
	}
	while (isdigit(*nptr)) {
		total *= 10;
		total += (*nptr++ - '0');
	}
	return minus ? -total : total;
}
