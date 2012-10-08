/*	@(#)ilb.c	1.2	94/04/06 11:18:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_ilb(l)
	label l;
{
	COMMA();
	ILB(l);
}
