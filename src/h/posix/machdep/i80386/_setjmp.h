/*	@(#)_setjmp.h	1.2	94/04/06 16:55:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef ___SETJMP_H__
#define ___SETJMP_H__

/*
** i80386-specific setjmp.h
** See src/lib/libc/machdep/i80386/sys/setjmp.s.
*/

typedef long jmp_buf[10];

#endif /* ___SETJMP_H__ */
