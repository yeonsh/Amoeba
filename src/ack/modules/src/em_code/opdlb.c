/*	@(#)opdlb.c	1.2	94/04/06 11:19:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_opdlb(opcode, dlb, offset)
	label dlb;
	arith offset;
{
	/*	Instruction that as a numeric datalabel + offset as argument
		Argument types: g
	*/
	OP(opcode);
	DOFF(dlb, offset);
	NL();
}
