/*	@(#)crdlb.c	1.2	94/04/06 11:14:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_crdlb(op, v, off)
	label v;
	arith off;
{
	/*	CON or ROM with argument DLB(v, off)
	*/
	PS(op);
	DOFF(v, off);
	CEND();
	NL();
}
