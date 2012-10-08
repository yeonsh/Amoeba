/*	@(#)head.s	1.4	96/02/27 10:58:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"

	AS_BEGIN
	SEG_TEXT

	.globl	start
start:
	save	%sp, -96, %sp

	call	GLNAME(_stackfix), 1
	mov	%i0, %o0		!! set argument

	ld	[%i0], %o0		! argc
	ld	[%i0+4], %o1		! argv
	ld	[%i0+8], %o2		! environ
	ld	[%i0+12], %o3		! capv

	sethi	%hi(GLNAME(environ)), %l1
	st	%o2, [%l1+%lo(GLNAME(environ))]
	sethi	%hi(GLNAME(capv)), %l1
	st	%o3, [%l1+%lo(GLNAME(capv))]

	call	GLNAME(main), 4
	nop				!!

	call	GLNAME(exit)
	nop				!!

	.word	0			! In case exit returns

	SEG_DATA

	.globl	GLNAME(_thread_local)
GLNAME(_thread_local):	.word -1
	.comm	GLNAME(environ),4
	.comm	GLNAME(capv),4

	!	The (gnu) linker now gets the bss right, so we don't need this
	!	pad buffer anymore:
	!	.comm	padbuffer, 65536
