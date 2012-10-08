/*	@(#)bhpnam.c	1.2	94/04/06 11:14:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_bhpnam(op, n, p, i)
	arith n;
	char *p;
	int i;
{
	/*	BSS or HOL with size n, initial value a PNAM(p),
		and flag i
	*/
	PS(op);
	CST(n);
	COMMA();
	PNAM(p);
	COMMA();
	CST((arith) i);
	NL();
}
