/*	@(#)int64.s	1.1	96/02/27 11:29:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"

	AS_BEGIN
	SEG_TEXT
	.align 8 

GLOBNAME(_copy_int64)
	! two pointer arguments pointing to 8 byte aligned integers
	! we copy the contents of first into the second
	ldd [%o0],%o2
	retl
	std %o2,[%o1]	!!
