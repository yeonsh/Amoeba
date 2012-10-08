/*	@(#)alloca.s	1.4	94/04/07 10:40:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
/*
 * Alloca - allocate space on the stack.
 *
 * This code assumes that the standard function prolog saves at most two
 * registers on the stack, and that AX-DX are available as scratch registers.
 */
	AS_BEGIN

	SEG_TEXT

	GLOBL GLNAME(alloca)
GLNAME(alloca):
	POP_L	(EBX)			/* Return address. */
	POP_L	(EAX)			/* Get size. */
	POP_L	(ECX)			/* Saved reg 1 */
	POP_L	(EDX)			/* Saved reg 2 */
	ADD_L	(CONST(3),EAX)
	AND_L	(CONST(0xfffffffc),EAX)	/* Round up to multiple of 4. */
	SUB_L	(EAX,ESP)		/* Reserve the space. */
	SUB_L	(CONST(8),ESP)		/* Plus the extra popped space */
	MOV_L	(ESP,EAX)		/* Set return value */
	PUSH_L	(EDX)			/* Restore saved reg 2 */
	PUSH_L	(ECX)			/* Restore saved reg 1 */
	PUSH_L	(EAX)			/* Will be popped by caller */
	PUSH_L	(EBX)			/* Return */
	RET
