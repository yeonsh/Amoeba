/*	@(#)strcpy.S	1.2	94/04/07 10:39:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * A moderately fast strcpy for the 386
 * Note that most compilers don't require us to save the ecx register.
 * If yours does then activate the ifdef.
 *
 * Author:  Gregory J. Sharp, Feb 1993
 */

#include "assyntax.h"

	AS_BEGIN
	SEG_TEXT

	GLOBL	GLNAME(strcpy)
GLNAME(strcpy):
	PUSH_L	(EDI)
	PUSH_L	(ESI)
#ifdef compiler_needs_you_to_save these
	PUSH_L	(ECX)
	MOV_L   (REGOFF(20, ESP), EDI)	/* src */
#else
	MOV_L   (REGOFF(16, ESP), EDI)	/* src */
#endif
	CLD

	/* Calculate the length of the source */
	XOR_L	(EAX, EAX)
	MOV_L	(CONST(-1), ECX)
	REPNZ SCAS_B

	/* We started the count at -1, so now we diddle it to find out
	 * what the string length was.
	 */
	INC_L	(ECX)
	NEG_L	(ECX)

	/* Now copy the string  ECX should hold the count */
#ifdef compiler_needs_you_to_save these
	MOV_L   (REGOFF(16, ESP), EDI)	/* dst */
	MOV_L   (REGOFF(20, ESP), ESI)	/* src */
#else
	MOV_L   (REGOFF(12, ESP), EDI)	/* dst */
	MOV_L   (REGOFF(16, ESP), ESI)	/* src */
#endif

	REP MOVS_B

#ifdef compiler_needs_you_to_save these
	MOV_L   (REGOFF(16, ESP), EAX)	/* Return original value of dst */
	POP_L	(ECX)
#else
	MOV_L	(REGOFF(12, ESP), EAX)	/* Return original value of dst */
#endif
	POP_L	(ESI)
	POP_L	(EDI)
	RET
