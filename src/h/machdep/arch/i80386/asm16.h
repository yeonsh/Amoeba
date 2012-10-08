/*	@(#)asm16.h	1.3	94/04/06 15:56:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ASM16_H__
#define __ASM16_H__

/*
 * 32-bit instructions for 16-bit mode
 */
#define	movl	a32 o32 mov
#define xorl	a32 o32 xor
#define addl	a32 o32 add
#define sall	a32 o32 sal
#define orl	a32 o32 or
#define pushl	a32 o32 push
#define popl	a32 o32 pop
#define shll	a32 o32 shl
#define shrl	a32 o32 shr
#define subl	a32 o32 sub
#define andl	a32 o32 and
#define jmpfl	a32 o32 jmpf

#endif /* __ASM16_H__ */
