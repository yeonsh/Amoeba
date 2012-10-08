/*	@(#)head.s	1.2	94/04/06 11:36:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

.globl	_read_kernel
start:
	jmp	start1
parvec:
	.long	415
start1:
	pea	parvec
	jsr	_read_kernel
	movl	d0,a0
	jmp	a0@
