/*	@(#)head.s	1.2	94/04/06 11:36:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

start:
	ba	skipvec
	nop
param:
	.word 0
skipvec:
	mov	%o0, %l0			! save romp
	sethi	%hi(param), %o1
	call	_readkernel
	ld	[%o1 + %lo(param)], %o1		!!
	mov	%o0, %l1
	jmp	%l1
	mov	%l0, %o0			!! restore romp
