/*	@(#)_setjmp.h	1.2	94/04/06 16:55:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef ___SETJMP_H__
#define ___SETJMP_H__

/*
** MC6800-specific setjmp.h.
** See src/lib/libc/machdep/mc68000/sys/setjmp.s.
*/

typedef long jmp_buf[20];

/* XXX Need to define sigjmp_buf as well */

#endif /* ___SETJMP_H__ */
