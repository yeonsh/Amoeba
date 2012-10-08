/*	@(#)bhilb.c	1.2	94/04/06 11:14:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_bhilb(op, n, l, i)
	arith n;
	label l;
	int i;
{
	/*	BSS or HOL with size n, initial value a ILB(l),
		and flag i
	*/
	PS(op);
	CST(n);
	COMMA();
	ILB(l);
	COMMA();
	CST((arith) i);
	NL();
}
