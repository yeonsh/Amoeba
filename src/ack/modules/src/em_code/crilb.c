/*	@(#)crilb.c	1.2	94/04/06 11:15:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_crilb(op, l)
	label l;
{
	/*	CON or ROM with argument ILB(l)
	*/
	PS(op);
	ILB(l);
	CEND();
	NL();
}
