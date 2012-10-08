/*	@(#)end.c	1.2	94/04/06 11:17:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_end(l)
	arith l;
{
	/*	END pseudo of procedure with l locals
	*/
	PS(ps_end);
	CST(l);
	NL();
}
