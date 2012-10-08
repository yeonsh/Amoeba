/*	@(#)exc.c	1.2	94/04/06 11:17:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_exc(c1,c2)
	arith c1,c2;
{
	PS(ps_exc);
	CST(c1);
	COMMA();
	CST(c2);
	NL();
}
