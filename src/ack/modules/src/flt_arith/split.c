/*	@(#)split.c	1.3	94/04/06 11:22:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */


#include "flt_misc.h"

flt_split(e, p)
	register flt_arith *e;
	register unsigned short *p;
{
	/*	Split mantissa of e into the array p
	*/

	p[0] = (int)(e->m1 >> 16) & 0xFFFF;
	p[1] = (int)(e->m1) & 0xFFFF;
	p[2] = (int)(e->m2 >> 16) & 0xFFFF;
	p[3] = (int)(e->m2) & 0xFFFF;
}
