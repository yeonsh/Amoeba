/*	@(#)icompute.c	1.3	96/03/15 14:06:57 */
/*
 * Copyright 1996 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * icompute.c - compute an integer
 */

#include	"loc_incl.h"

/* This routine is used in doprnt.c as well as in tmpfile.c and tmpnam.c. */

char *
_i_compute(val, base, s, nrdigits)
unsigned long val;
int base;
char *s;
int nrdigits;
{
	int c;

	c= val % base ;
	val /= base ;
	if (val || nrdigits > 1)
		s = _i_compute(val, base, s, nrdigits - 1);
	*s++ = (c>9 ? c-10+'a' : c+'0');
	return s;
}
