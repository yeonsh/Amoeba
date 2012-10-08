/*	@(#)sqrt.s	1.1	96/02/27 11:13:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"

/* Use the sparc (v8) hardware square root instruction, unless an error
 * is detected, in which case we jump to the general routine.
 * That will take care of returning an apprioriate error value and
 * setting errno.
 */
	AS_BEGIN
	SEG_TEXT
	.align 8

GLOBNAME(sqrt)
	/* Put the value on the stack while staying a leaf function */
	sub %sp, 96, %sp
	std %o0,[%sp+88]
	ldd [%sp+88],%f2
	add %sp, 96, %sp
	fsqrtd %f2, %f0
	fcmpd  %f0, %f0
	nop
	/* Error cases (NaN, overflow, Inf) are handled by sqrt_gen. */
	fbne GLNAME(sqrt_gen)
	  nop
	/* No problem, %f0 contains the square root return value */
	retl
	  nop
