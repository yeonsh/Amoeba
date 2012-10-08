/*	@(#)opdnam.c	1.2	94/04/06 11:19:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_opdnam(opcode, dnam, offset)
	char *dnam;
	arith offset;
{
	/*	Instruction that has a datalabel + offset as argument
		Argument types: g
	*/
	OP(opcode);
	NOFF(dnam, offset);
	NL();
}
