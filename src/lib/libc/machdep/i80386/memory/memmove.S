/*	@(#)memmove.S	1.3	94/04/07 10:39:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * memmove.S
 *
 * Fast memmove implementation (using the i80386 move string instructions).
 *
 * Author:
 *	Leendert van Doorn
 */
#include <assyntax.h>

	AS_BEGIN

	SEG_TEXT

/*
 * memmove(dst, src, cnt)
 */
	GLOBL	GLNAME(memmove)
	GLOBL	GLNAME(memcpy)
GLNAME(memmove):
GLNAME(memcpy):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(ECX)
	PUSH_L	(ESI)
	PUSH_L	(EDI)
	MOV_L	(REGOFF(8,EBP),EDI)	/* dst */
	MOV_L	(REGOFF(12,EBP),ESI)	/* src */
	MOV_L	(REGOFF(16,EBP),ECX)	/* count */
	OR_L	(ECX,ECX)		/* when count == 0 */
	JZ	(memmove2)
	CMP_L	(ESI,EDI)
	JBE	(memmove1)
	STD
	ADD_L	(ECX,ESI)		/* copy down wards */
	ADD_L	(ECX,EDI)
	AND_L	(CONST(3),ECX)
	DEC_L	(EDI)
	DEC_L	(ESI)
	REP	MOVS_B
	MOV_L	(REGOFF(16,EBP),ECX)	/* count */
	SHR_L	(CONST(2),ECX)
	SUB_L	(CONST(3),ESI)
	SUB_L	(CONST(3),EDI)
	REP	MOVS_L			/* long words */
	CLD
	JMP	(memmove2)
memmove1:
	CLD				/* copy forward */
	SHR_L	(CONST(2),ECX)
	REP	MOVS_L			/* long words */
	MOV_L	(REGOFF(16,EBP),ECX)	/* count */
	AND_L	(CONST(3),ECX)
	REP	MOVS_B			/* byte copy */
memmove2:
	MOV_L	(REGOFF(8,EBP),EAX)	/* return dst */
	POP_L	(EDI)
	POP_L	(ESI)
	POP_L	(ECX)
	POP_L	(EBP)
	RET
