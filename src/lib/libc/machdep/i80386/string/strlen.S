/*	@(#)strlen.S	1.2	94/04/07 10:39:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * A moderately fast strlen for the 386
 * Note that most compilers don't require us to save the ecx & edx registers.
 * If yours does then activate the ifdef.
 *
 * Author:  Gregory J. Sharp, Feb 1993
 */

#include "assyntax.h"

	AS_BEGIN
	SEG_TEXT

	GLOBL	GLNAME(strlen)
GLNAME(strlen):
	PUSH_L	(EDI)
#ifdef compiler_needs_you_to_save these
	PUSH_L	(ECX)
	PUSH_L	(EDX)
	MOV_L   (REGOFF(16, ESP), EDI)
#else
	MOV_L   (REGOFF(8, ESP), EDI)
#endif

	MOV_L	(EDI, EDX)
	XOR_L	(EAX,EAX)		/* Compare with 0 */
	MOV_L	(CONST(-1), ECX)	/* Count = -1 == infinite */
	CLD
	REPNZ SCAS_B			/* Search for 0 byte */
	LEA_L	(REGOFF(-1, EDI), EAX)	/* Calculate sz = endp - beginp - 1 */
	SUB_L	(EDX, EAX)

#ifdef compiler_needs_you_to_save these
	POP_L	(EDX)
	POP_L	(ECX)
#endif
	POP_L	(EDI)
	RET
