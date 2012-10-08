/*	@(#)memset.S	1.2	94/04/07 10:39:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *  An assembler version of memset - slightly quicker than the C version.
 *
 *  Author: Gregory J. Sharp, Feb 1993
 */

#include "assyntax.h"

	AS_BEGIN
	SEG_TEXT

	GLOBL	GLNAME(memset)
GLNAME(memset):
	PUSH_L	(EBP)
	MOV_L	(ESP, EBP)
	PUSH_L	(EDI)
#ifdef compiler_needs_you_to_save_these
	PUSH_L	(ECX)
	PUSH_L	(EDX)
#endif
	CLD

	MOV_L	(REGOFF( 8, EBP), EDI)	/* dst */
	MOV_B	(REGOFF(12, EBP), AL)	/* value */
	MOV_L	(REGOFF(16, EBP), ECX)	/* Byte count */

	/*
	 * If the count is sufficiently large it is probably worth
	 * trying to do 4 bytes at a time.
	 */

	CMP_L	(CONST(20), ECX)	/* If there are fewer than 20 bytes */
	JLE	(tail)			/* then don't fuss with 4 byte moves */

	/* Make a 4 byte version of the value to write. */
	MOV_B	(AL, AH)		/* Bottom 16 bits of EAX are set */
	SHRD_L	(CONST(16), EAX, EDX)	/* Set top 16 bits of EDX */
	SHLD_L	(CONST(16), EDX, EAX)	/* EAX now has 4 copies of the */
					/* 2nd argument */
	
	/* Ensure 4 byte alignment */
	MOV_L	(ECX, EDX)	/* Save byte count */
	MOV_L	(EDI, ECX)	/* Get the pointer into ECX to look at low */
	NEG_L	(ECX)		/* two bits.  We calulate 4 - (dst & 3) */
	AND_L	(CONST(3), ECX)	/* which is # bytes to copy till aligned */
	SUB_L	(ECX, EDX)
	REP STOS_B

	/* Now we are 4 byte aligned */
	MOV_L	(EDX, ECX)	/* restore byte count */
	SHR_L	(CONST(2), ECX)	/* convert byte count to word count */
	REP STOS_L		/* and store madly */

	/* Now we might have some leftover unaligned bytes at the end */
	AND_L	(CONST(3), EDX)	/* Look at low bits of byte count */
	MOV_L	(EDX, ECX)

tail:
	/* Copy the last few bytes */
	REP STOS_B
	MOV_L   (REGOFF(8, EBP), EAX)

#ifdef compiler_needs_you_to_save_these
	POP_L	(EDX)
	POP_L	(ECX)
#endif
	POP_L	(EDI)
	POP_L	(EBP)
	RET
