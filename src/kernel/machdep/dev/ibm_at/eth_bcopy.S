/*	@(#)eth_bcopy.S	1.2	94/04/06 09:18:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#if !defined(NO3C) || !defined(NOWD)

/*
 * eth_bcopy.S
 *
 *	Used by Western Digital and 3 Com ethernet drivers.
 *
 * Author:	Leendert van Doorn
 * Modified:	Gregory J. Sharp, Oct 1992 - converted to assyntax.h
 */
#include <assyntax.h>

	AS_BEGIN
	SEG_TEXT

/*
 * eth_bcopy(inwords, src, dst, count) copies a block of memory using
 * byte or 16-bit word move instructions. Its primary use is to copy
 * data from and to the ethernet on-board packet buffer.
 */
	GLOBL GLNAME(eth_bcopy)
GLNAME(eth_bcopy):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(ECX)
	PUSH_L	(EBX)
	PUSH_L	(ESI)
	PUSH_L	(EDI)
	MOV_L	(REGOFF(12,EBP),ESI)		/* src */
	MOV_L	(REGOFF(16,EBP),EDI)		/* dst */
	MOV_L	(REGOFF(20,EBP),ECX)		/* count */
	CMP_L	(CONST(0),REGOFF(8,EBP))	/* byte or word move */
	JE	(eth_bcopy1)
	MOV_L	(ECX,EBX)
	SHR_L	(CONST(1),ECX)
	REP	MOVS_W				/* words */
	MOV_L	(EBX,ECX)
	AND_L	(CONST(1),ECX)
eth_bcopy1:
	REP	MOVS_B				/* bytes */
	POP_L	(EDI)
	POP_L	(ESI)
	POP_L	(EBX)
	POP_L	(ECX)
	POP_L	(EBP)
	RET

#endif
