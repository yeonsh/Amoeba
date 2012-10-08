/*	@(#)crdnam.c	1.2	94/04/06 11:15:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_crdnam(op, s, off)
	char *s;
	arith off;
{
	/*	CON or ROM with argument DNAM(s, off)
	*/
	PS(op);
	NOFF(s, off);
	CEND();
	NL();
}
