/*	@(#)bhdnam.c	1.2	94/04/06 11:13:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_bhdnam(op, n, s, off, i)
	arith n;
	char *s;
	arith off;
	int i;
{
	/*	BSS or HOL with size n, initial value a dnam(s, off),
		and flag i
	*/
	PS(op);
	CST(n);
	COMMA();
	NOFF(s, off);
	COMMA();
	CST((arith) i);
	NL();
}
