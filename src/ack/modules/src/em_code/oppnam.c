/*	@(#)oppnam.c	1.2	94/04/06 11:20:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_oppnam(opcode, pnam)
	char *pnam;
{
	/*	Instruction that has a procedure name as argument
		Argument types: p
	*/
	OP(opcode);
	PNAM(pnam);
	NL();
}
