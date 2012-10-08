/*	@(#)memmove.s	1.2	94/04/07 10:41:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"

AS_BEGIN
GLOBLDIR _memmove
GLOBLDIR _memcpy

COM	memmove(dst, src, byte_count) -> dst
COM	memcpy(dst, src, byte_count) -> dst

_memmove:
_memcpy:
	MOVAL	REGOFFSETTED(sp,4),a0	COM  dst -> a0
	MOVAL	REGOFFSETTED(sp,8),a1	COM  src -> a1
	MOVL	REGOFFSETTED(sp,12),d0	COM  len -> d0
	JLE	return			COM  if (len <= 0) return
	CMPAL	a0,a1			COM  if (src <= dst)
	JHI	copy_forward
					COM  copy backwards
	ADDAL	d0,a1			COM    src += len
	ADDAL	d0,a0			COM    dst += len
	MOVL	a1,d1			COM    if (odd_address(src))
	btst	#0,d1			COM    {
	JEQ	L16
	MOVB	AUTODEC(a1),AUTODEC(a0)	COM       *--dst = *--src;
	SUBQL	#1,d0			COM	  --len;
L16:					COM    }
	MOVL	a0,d1			COM    if (even_address(dst))
	btst	#0,d1			COM    {   then src and dst are even
	JNE	L20
	MOVL	d0,d1			COM        x = len >>2
	ASRL	#2,d1
	JRA	LY00000			COM	   while (--x >= 0)
LY00001:
	MOVL	AUTODEC(a1),AUTODEC(a0)	COM           *--((long *) dst) =
LY00000:				COM                *--((long *) src);
	SUBQL	#1,d1
	JPL	LY00001
	MOVEQ	#3,d1			COM        len &= 3;
	ANDL	d1,d0			COM    }
L20:
	SUBQL	#1,d0			COM    while (--len >= 0)
	JMI	return
	MOVB	AUTODEC(a1),AUTODEC(a0)	COM       *--dst = *--src;
	JRA	L20
copy_forward:
	MOVL	a1,d1			COM    if (odd_address(src))
	btst	#0,d1			COM    {
	JEQ	L23
	MOVB	AUTOINC(a1),AUTOINC(a0)	COM        *dst++ = *src++;
	SUBQL	#1,d0			COM	   --len;
L23:					COM    }
	MOVL	a0,d1			COM    if (even_address(src))
	btst	#0,d1			COM    {   then src and dst are even
	JNE	LY00004
	MOVL	d0,d1			COM        x = len >> 2;
	ASRL	#2,d1
	JRA	LY00002			COM        while (--x >= 0)
LY00003:
	MOVL	AUTOINC(a1),AUTOINC(a0)	COM           *((long *) dst)++ =
LY00002:				COM                  *((long *) src)++;
	SUBQL	#1,d1
	JPL	LY00003
	MOVEQ	#3,d1			COM        len &= 3;
	ANDL	d1,d0			COM    }
	JRA	LY00004
LY00005:				COM    while (--len >= 0)
	MOVB	AUTOINC(a1),AUTOINC(a0)	COM       *dst++ = *src;
LY00004:
	SUBQL	#1,d0
	JPL	LY00005
return:
	MOVL	REGOFFSETTED(sp,4),d0	COM  dst -> d0 (must return dst)
	rts
