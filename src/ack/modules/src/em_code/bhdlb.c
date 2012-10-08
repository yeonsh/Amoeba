/*	@(#)bhdlb.c	1.2	94/04/06 11:13:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_bhdlb(op, n, s, off, i)
	arith n;
	label s;
	arith off;
	int i;
{
	/*	BSS or HOL with size n, initial value a dlb(s, off),
		and flag i
	*/
	PS(op);
	CST(n);
	COMMA();
	DOFF(s, off);
	COMMA();
	CST((arith) i);
	NL();
}
