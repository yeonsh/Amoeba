/*	@(#)opcst.c	1.2	94/04/06 11:19:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_opcst(opcode, cst)
	arith cst;
{
	/*	Instruction with a constant argument
		Argument types: c, d, l, g, f, n, s, z, o, w, r
	*/
	OP(opcode);
	CST(cst);
	NL();
}
