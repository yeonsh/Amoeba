/*	@(#)setjmp.s	1.5	96/02/29 16:47:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: setjmp.s -- contains the setjmp() and longjmp() routines. These are
 * pretty straight forward, but some information might be needed on way
 * this is needed: The sparc assembler is capable (and willing) to move
 * register assignments (for local and temporary variables) across procedure
 * calls. So we must save *all* registers in the caller's window (ins and
 * locals), and the stack pointer and return address (from the
 * output registers). The only trick we use here is to look at longjmp()
 * caller's stack chain, and just follow it down to the 'sp' saved in 
 * the jmpbuf.
 */

#include "assyntax.h"

#define	MAX_WINDOWS	32		/* Should be okay for a while */
#define MINFRAME	60		/* Should include header file */

	AS_BEGIN
	SEG_TEXT

GLOBNAME(setjmp)
	st	%o7, [ %o0 ]
	st	%o6, [ %o0 + 4 ]
	st	%l0, [ %o0 + 8 ]	! possibly unaligned, so do not use std
	st	%l1, [ %o0 + 12 ]
	st	%l2, [ %o0 + 16 ]
	st	%l3, [ %o0 + 20 ]
	st	%l4, [ %o0 + 24 ]
	st	%l5, [ %o0 + 28 ]
	st	%l6, [ %o0 + 32 ]
	st	%l7, [ %o0 + 36 ]
	st	%i0, [ %o0 + 40 ]
	st	%i1, [ %o0 + 44 ]
	st	%i2, [ %o0 + 48 ]
	st	%i3, [ %o0 + 52 ]
	st	%i4, [ %o0 + 56 ]
	st	%i5, [ %o0 + 60 ]
	st	%i6, [ %o0 + 64 ]
	st	%i7, [ %o0 + 68 ]

	retl
	mov	%g0, %o0		!!

GLOBNAME(longjmp)
	/* First flush the current register windows to the stack by doing
	 * a _sys_null system call (undocumented feature; maybe the name
	 * should change).  We need this for the subsequent stack unwind
	 * to happen correctly.
	 */
	call	GLNAME(_sys_null)
	nop				!!

	mov	%o0, %g1
	orcc 	%g0, %o1, %g2		! when longjmp val is 0, return 1
	be,a	0f
	mov	1, %g2

0:	ld	[ %g1 + 4 ], %g3

	cmp	%o6, %g3
	be	lj_gotit
	nop				!!

lj_looking:
	cmp	%i6, %g3
	bge	lj_looking
	restore				!!

lj_gotit:
	ld	[ %g1 ], %o7
	ld	[ %g1 + 4 ], %o6
	ld	[ %g1 + 8 ], %l0
	ld	[ %g1 + 12 ], %l1
	ld	[ %g1 + 16 ], %l2
	ld	[ %g1 + 20 ], %l3
	ld	[ %g1 + 24 ], %l4
	ld	[ %g1 + 28 ], %l5
	ld	[ %g1 + 32 ], %l6
	ld	[ %g1 + 36 ], %l7
	ld	[ %g1 + 40 ], %i0
	ld	[ %g1 + 44 ], %i1
	ld	[ %g1 + 48 ], %i2
	ld	[ %g1 + 52 ], %i3
	ld	[ %g1 + 56 ], %i4
	ld	[ %g1 + 60 ], %i5
	ld	[ %g1 + 64 ], %i6
	ld	[ %g1 + 68 ], %i7

	retl
	mov	%g2, %o0		!!
