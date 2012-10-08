/*	@(#)fltpr.c	1.3	96/03/15 14:06:48 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * floatpr.c - print floating point numbers
 */

#ifndef	NOFLOAT
#include	"loc_incl.h"

static char *
_pfloat(r, s, n, flags)
LONG_DOUBLE r;
register char *s;
int n;
int flags;
{
	register char *s1;
	int sign, dp;
	register int i;

	s1 = _fcvt(r, n, &dp, &sign);
	if (sign)
		*s++ = '-';
	else if (flags & FL_SIGN)
		*s++ = '+';
	else if (flags & FL_SPACE)
		*s++ = ' ';

	if (dp<=0)
		*s++ = '0';
	for (i=dp; i>0; i--)
		if (*s1) *s++ = *s1++;
		else *s++ = '0';
	if (((i=n) > 0) || (flags & FL_ALT))
		*s++ = '.';
	while (++dp <= 0) {
		if (--i<0)
			break;
		*s++ = '0';
	}
	while (--i >= 0)
		if (*s1) *s++ = *s1++;
		else *s++ = '0';
	return s;
}

static char *
_pscien(r, s, n, flags)
LONG_DOUBLE r;
register char *s;
int n;
int flags;
{
	int sign, dp; 
	register char *s1;

	s1 = _ecvt(r, n + 1, &dp, &sign);
	if (sign)
		*s++ = '-';
	else if (flags & FL_SIGN)
		*s++ = '+';
	else if (flags & FL_SPACE)
		*s++ = ' ';

	*s++ = *s1++;
	if ((n > 0) || (flags & FL_ALT))
		*s++ = '.';
	while (--n >= 0)
		if (*s1) *s++ = *s1++;
		else *s++ = '0';
	*s++ = 'e';
	if ( r != 0 ) --dp ;
	if ( dp<0 ) {
		*s++ = '-' ; dp= -dp ;
	} else {
		*s++ = '+' ;
	}
	if (dp >= 100) {
		*s++ = '0' + (dp / 100);
		dp %= 100;
	}
	*s++ = '0' + (dp/10);
	*s++ = '0' + (dp%10);
	return s;
}

#ifdef __STDC__
char *
_f_print(va_list *ap, int flags, char *s, char c, int precision)
#else /* __STDC__ */
char *
_f_print(ap, flags, s, c, precision)
va_list *ap;
int flags;
char *s;
char c;
int precision;
#endif /* __STDC__ */
{
	register char *old_s = s;
	LONG_DOUBLE ld_val;

	if (flags & FL_LONGDOUBLE) ld_val = va_arg(*ap, LONG_DOUBLE);
	else ld_val = (LONG_DOUBLE) va_arg(*ap, double);

	switch(c) {
	case 'f':
		s = _pfloat(ld_val, s, precision, flags);
		break;
	case 'e':
	case 'E':
		s = _pscien(ld_val, s, precision , flags);
		break;
	case 'g':
	case 'G':
		s = _gcvt(ld_val, precision, s, flags);
		s += strlen(s);
		break;
	}
	if ( c == 'E' || c == 'G') {
		while (*old_s && *old_s != 'e') old_s++;
		if (*old_s == 'e') *old_s = 'E';
	}
	return s;
}
#endif	/* NOFLOAT */
