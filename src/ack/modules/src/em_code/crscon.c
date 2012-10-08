/*	@(#)crscon.c	1.2	94/04/06 11:15:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_crscon(op, v, s)
	char *v;
	arith s;
{
	/*	CON or ROM with argument SCON(v,z)
	*/
	PS(op);
	SCON(v, s);
	CEND();
	NL();
}
