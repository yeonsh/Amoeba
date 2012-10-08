/*	@(#)loadstore.s	1.3	96/02/27 11:29:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"

/*
 * support for the new mutex implementation
 */
	AS_BEGIN
	SEG_TEXT
	.align 8

GLOBNAME(__ldstub)
	ldstub	[%o0],%o1
	retl
	mov	%o1,%o0		!!
