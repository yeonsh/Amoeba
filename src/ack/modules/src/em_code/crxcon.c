/*	@(#)crxcon.c	1.2	94/04/06 11:15:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_crxcon(op, spec, v, s)
	char *v;
	arith s;
{
	/*	CON or ROM with argument ICON(v,z)
	*/
	PS(op);
	WCON(spec, v, s);
	CEND();
	NL();
}
