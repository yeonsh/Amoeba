/*	@(#)stret1.s	1.2	94/04/07 10:44:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Routine to handle structure returns for routines with the bad taste
 * to pass structs as parameters to functions
 */

	.text
	.globl	.stret1

.stret1:
	ld	[%i7 + 0x8], %o4
	and	%o1, 0xFFF, %o5
	cmp	%o5, %o4
	bne	9f
	ld	[%fp + 0x40], %i0
8:
	subcc	%o1, 0x1, %o1
	ldub	[%o0 + %o1], %o4
	bg	8b
	stb	%o4, [%i0 + %o1]
	add	%i7, 0x4, %i7
9:
	ret
	restore
