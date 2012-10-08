/*	@(#)memset.S	1.4	96/02/27 11:10:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *  void * memset(void * s, int c, size_t n)
 *	This version uses double stores to gain some speed.  If you're
 *	really keen you could unroll the loop but it doesn't gain much
 *	because the cost of the loop control is small compared to the stores.
 *
 *  Author: Gregory J. Sharp, July 1993
 *
 */
#include "assyntax.h"

	AS_BEGIN
	SEG_TEXT

GLOBNAME(memset)
	and	%o1, 0xff, %o4	!   c1 = c & 0xff;
	cmp	%o2, 0xf
	bleu	last_part
	mov	%o0, %o3	!!  s1 = (char *) s;
	andcc	%o3, 7, %g0	!   check if s1 is 8 byte aligned
	be	no_preamble
	sll	%o4, 0x8, %o5	!!  put c into all 4 bytes of c1
preamble:			!   make s1 8 byte aligned
	stb	%o4, [%o3]	!	if (s1 & 7)
	add	%o3, 1, %o3	!	    do {
	andcc	%o3, 7, %g0	!		*s1++ = c1;
	bne	preamble	!		n--;
	sub	%o2, 1, %o2	!!	    } while (s1 & 7);
no_preamble:
	or	%o4, %o5, %o4	! finish putting c into all 4 bytes of c1
	sll	%o4, 0x10, %o5
	or	%o4, %o5, %o4
	mov	%o4, %o5	!   put c1 into c2 so we have 8 bytes worth
	andn	%o2, 7, %o1	!  x = c & ~7 == # bytes 8 byte aligned
loop_8_byte:
	std	%o4, [%o3]	!   do {
	subcc	%o1, 0x8, %o1	!	*(int64 *)s1++ = c1&c2;
	bne	loop_8_byte	!   } while (--x != 0);
	add	%o3, 0x8, %o3	!!
	and	%o2, 7, %o2	!   n &= 7;
last_part:
	orcc	%g0, %o2, %g0	!   if (n != 0)
	be	done
	nop			!!
last_loop:
	stb	%o4, [%o3]	!   do {
	subcc	%o2, 1, %o2	!       *s1++ = c1;
	bne	last_loop	!   } while (--n != 0);
	add	%o3, 1, %o3	!!
done:
	retl			!   return s;
	nop
