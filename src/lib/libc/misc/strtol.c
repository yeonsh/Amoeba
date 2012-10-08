/*	@(#)strtol.c	1.2	93/06/10 11:07:54 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include	<ctype.h>
#ifndef KERNEL
#include	<errno.h>
#endif
#include	<limits.h>
#include	<stdlib.h>
#include	"_ARGS.h"

#ifndef __STDC__
#define const  /*const*/
#define signed /*signed*/
#endif

static unsigned long
string2long _ARGS((register const char *nptr, char **endptr,
			int base, int is_signed));

long int
strtol(nptr, endptr, base)
register const char *nptr;
char **endptr;
int base;
{
	return (signed long)string2long(nptr, endptr, base, 1);
}

unsigned long int
strtoul(nptr, endptr, base)
register const char *nptr;
char **endptr;
int base;
{
	return (unsigned long)string2long(nptr, endptr, base, 0);
}

static unsigned long
string2long(nptr, endptr, base, is_signed)
register const char *nptr;
char ** const endptr;
int base;
int is_signed;
{
	register int v;
	register unsigned long val = 0;
	register int c;
	int ovfl = 0, sign = 1;
	const char *startnptr = nptr, *nrstart;

	if (endptr) *endptr = (char *)nptr;
	while (isspace(*nptr)) nptr++;
	c = *nptr;

	if (c == '-' || c == '+') {
		if (c == '-') sign = -1;
		nptr++;
	}
	nrstart = nptr;			/* start of the number */

	/* When base is 0, the syntax determines the actual base */
	if (base == 0)
		if (*nptr == '0')
			if (*++nptr == 'x' || *nptr == 'X') {
				base = 16;
				nptr++;
			}
			else	base = 8;
		else	base = 10;
	else if (base==16 && *nptr=='0' && (*++nptr =='x' || *nptr =='X'))
		nptr++;

	while (isdigit(c = *nptr) || isalpha(c)) {
		if (!ovfl) {
			if (isalpha(c))
				v = 10 + (isupper(c) ? c - 'A' : c - 'a');
			else
				v = c - '0';
			if (v >= base) break;
			if (val > (ULONG_MAX - v) / (unsigned)base) ovfl++;
			val = (val * base) + v;
		}
		nptr++;
	}
	if (endptr) {
		if (nrstart == nptr) *endptr = (char *)startnptr;
		else *endptr = (char *)nptr;
	}

	if (!ovfl) {
		/* Overflow is only possible when converting a signed long.
		 * The expression "-(LONG_MIN+1) + (unsigned long)1" is used
		 * to specify "(unsigned long) -LONG_MIN" in a portable way.
		 */
		if (is_signed
		    && (   (sign < 0 && val > -(LONG_MIN+1) + (unsigned long)1)
			|| (sign > 0 && val > LONG_MAX)))
		    ovfl++;
	}

	if (ovfl) {
#ifndef KERNEL
		errno = ERANGE;
#endif
		if (is_signed)
			if (sign < 0) return LONG_MIN;
			else return LONG_MAX;
		else return ULONG_MAX;
	}
	return (long) sign * val;
}
