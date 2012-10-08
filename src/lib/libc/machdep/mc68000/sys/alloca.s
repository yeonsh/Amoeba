/*	@(#)alloca.s	1.2	94/04/07 10:41:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"

AS_BEGIN
COM
COM Alloca - allocate space on the stack.
COM

GLOBLDIR	_alloca

_alloca:
	MOVAL	REGINDIR(sp), a0	COM Get return address
	MOVL	REGOFFSETTED(sp,4), d0	COM Get size
	ADDQW	#3, d0
	ANDL	#0xfffffffc, d0		COM Round up
	SUBAL	d0,sp			COM Reserve the space
	MOVL	sp, d0
	ADDQW	#8, d0			COM compensate
	jmp	REGINDIR(a0)		COM return
