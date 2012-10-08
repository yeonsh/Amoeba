/*	@(#)asm32.h	1.2	94/04/06 15:56:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ASM32_H__
#define __ASM32_H__

/*
 * 16-bit instructions for 32-bit mode
 */
#define	movw	o16 mov
#define xorw	o16 xor
#define addw	o16 add
#define salw	o16 sal
#define orw	o16 or
#define shlw	o16 shl
#define shrw	o16 shr
#define subw	o16 sub
#define andw	o16 and

#endif /* __ASM32_H__ */
