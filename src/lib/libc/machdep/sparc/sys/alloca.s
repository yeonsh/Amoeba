/*	@(#)alloca.s	1.4	96/02/27 11:10:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * alloca.s -- user-mode stack based memory allocator. This function simply
 * pulls down the stack pointer and copies the data in the frame to a
 * lower address and returns the memory opened at the top. Note that the
 * memory allocated must be rounded up to 8 byte boundary to not unalign
 * the stack. Not also that the address of the memory to be returned is
 * found by just subtracting the amount from the window's *frame* pointer.
 */


/*
 * The SunOS compiler does some of this work for us. If hands us the start
 * and stop addresses of the register window's temporary area, which seems
 * to be only used for stashing temporary variables during evaluation
 * of very complex expressions. We only need to copy the data between
 * "%sp+%o1" to "%sp+%o2", and the compiler even aligns %o0 for us.
 *
 *  In C it looks like:
 *	char * __builtin_alloca(size, offset1, offset2)
 *	{
 *	    extern char * sp;
 *	    char * oldsp = sp;
 *	    int gap;
 *
 *	    sp -= size;
 *	    for (gap = offset2 - offset1; gap > 0; offset1 += 4, gap -= 4)
 *		*(int *)(sp + offset1) = *(int *) (oldsp + offset1);
 *	    return (char *) sp + offset2;
 *	}
 */

#include "assyntax.h"

	AS_BEGIN
	SEG_TEXT

GLOBNAME(__builtin_alloca)
	mov	%o6, %o5		! oldsp = sp
	sub	%o6, %o0, %o6		! sp += size;
	subcc	%o2, %o1, %o4		! gap = offset2 - offset1
	be	done			! gap > 0?
	subcc	%o4, 4, %o4		!! gap -= 4

ba_copy:
	ld	[ %o5 + %o1 ], %o3	! x = *(oldsp + offset1)
	st	%o3, [ %o6 + %o1 ]	! *(sp + offset1) = x
	add	%o1, 4, %o1		! offset1 += 4
	bnz	ba_copy
	subcc	%o4, 4, %o4		!! gap -= 4

done:
	retl
	add	%o6, %o2, %o0		!! return sp + offset2
