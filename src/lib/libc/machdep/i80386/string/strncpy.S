/*	@(#)strncpy.S	1.2	94/04/07 10:39:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * A moderately fast strncpy for the 386
 * Note that most compilers don't require us to save the ecx register.
 * If yours does then activate the ifdef.
 *
 * Author:  Gregory J. Sharp, Feb 1993
 */

#include "assyntax.h"

	AS_BEGIN
	SEG_TEXT

	GLOBL	GLNAME(strncpy)
GLNAME(strncpy):
	PUSH_L	(EDI)
	PUSH_L	(ESI)
#ifdef compiler_needs_you_to_save these
	PUSH_L	(ECX)
	MOV_L   (REGOFF(16, ESP), EDI)	/* dst */
	MOV_L   (REGOFF(20, ESP), ESI)	/* src */
	MOV_L	(REGOFF(24, ESP), ECX)	/* count */
#else
	MOV_L   (REGOFF(12, ESP), EDI)	/* dst */
	MOV_L   (REGOFF(16, ESP), ESI)	/* src */
	MOV_L	(REGOFF(20, ESP), ECX)	/* count */
#endif

	TEST_L	(ECX, ECX)	/* if count == 0 then we are done */
	JE	(2f)
1:
	LODS_B			/* load byte into AL & increment ESI */
	STOS_B			/* store byte from AL & increment EDI */
	TEST_B	(AL, AL)	/* if AL is 0 we hit the end of string */
	LOOPNZ	(1b)		/* or if the count in ecx ran out we stop */
	CLD
	REP STOS_B		/* Just in case the count didn't run out we */
				/* pad with null bytes */
2:
#ifdef compiler_needs_you_to_save these
	MOV_L   (REGOFF(16, ESP), EAX)	/* Return original value of dst */
	POP_L	(ECX)
#else
	MOV_L   (REGOFF(12, ESP), EAX)	/* Return original value of dst */
#endif
	POP_L	(ESI)
	POP_L	(EDI)
	RET
