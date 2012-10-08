/*	@(#)opnarg.c	1.2	94/04/06 11:20:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_opnarg(opcode)
{
	/*	Instruction with optional argument, but now without one
		Argument types: w
	*/
	OP(opcode);
	CCEND();
	NL();
}
