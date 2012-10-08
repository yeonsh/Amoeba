/*	@(#)ucon.c	1.2	94/04/06 11:21:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_ucon(val,siz)
	char *val;
	arith siz;
{
	COMMA();
	WCON(sp_ucon, val, siz);
}
