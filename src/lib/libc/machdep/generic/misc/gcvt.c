/*	@(#)gcvt.c	1.2	94/04/07 10:37:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * gcvt.c - conversion for printing a floating point number
 */

#ifndef NOFLOAT
#include	"../../../stdio/loc_incl.h"

#define	NDIGINEXP(exp)		(((exp) >= 100 || (exp) <= -100) ? 3 : 2)
#define	LOW_EXP			-4
#define	USE_EXP(exp, ndigits)	(((exp) < LOW_EXP + 1) || (exp >= ndigits + 1))

char *
_gcvt(value, ndigit, s, flags)
LONG_DOUBLE value;
int ndigit;
char *s;
int flags;
{
	int sign, dp;
	register char *s1, *s2;
	register int i;
	register int nndigit = ndigit;

	s1 = _ecvt(value, ndigit, &dp, &sign);
	s2 = s;
	if (sign) *s2++ = '-';
	else if (flags & FL_SIGN)
		*s2++ = '+';
	else if (flags & FL_SPACE)
		*s2++ = ' ';

	if (!(flags & FL_ALT))
		for (i = nndigit - 1; i > 0 && s1[i] == '0'; i--)
			nndigit--;

	if (USE_EXP(dp,ndigit))	{
		/* Use E format */
		dp--;
		*s2++ = *s1++;
		if ((nndigit > 1) || (flags & FL_ALT)) *s2++ = '.';
		while (--nndigit > 0) *s2++ = *s1++;
		*s2++ = 'e';
		if (dp < 0) {
			*s2++ = '-';
			dp = -dp;
		}
		else	 *s2++ = '+';
		s2 += NDIGINEXP(dp);
		*s2 = 0;
		for (i = NDIGINEXP(dp); i > 0; i--) {
			*--s2 = dp % 10 + '0';
			dp /= 10;
		}
		return s;
	}
	/* Use f format */
	if (dp <= 0) {
		if (*s1 != '0')	{
			/* otherwise the whole number is 0 */
			*s2++ = '0';
			*s2++ = '.';
		}
		while (dp < 0) {
			dp++;
			*s2++ = '0';
		}
	}
	for (i = 1; i <= nndigit; i++) {
		*s2++ = *s1++;
		if (i == dp) *s2++ = '.';
	}
	if (i <= dp) {
		while (i++ <= dp) *s2++ = '0';
		*s2++ = '.';
	}
	if ((s2[-1]=='.') && !(flags & FL_ALT)) s2--;
	*s2 = '\0';
	return s;
}
#else
#ifndef lint
static int file_may_not_be_empty;
#endif
#endif	/* NOFLOAT */
