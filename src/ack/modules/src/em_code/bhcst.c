/*	@(#)bhcst.c	1.2	94/04/06 11:13:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_bhcst(op, n, w, i)
	arith n;
	arith w;
	int i;
{
	/*	BSS or HOL with size n, initial value a cst w, and flag i
	*/
	PS(op);
	CST(n);
	COMMA();
	CST(w);
	COMMA();
	CST((arith) i);
	NL();
}
