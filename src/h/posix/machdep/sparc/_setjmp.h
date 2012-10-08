/*	@(#)_setjmp.h	1.2	94/04/06 16:55:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: _setjmp.h -- SPARC-specific setjmp.h.
 * See src/lib/libc/machdep/sparc/sys/setjmp.s.
 */

#ifndef ___SETJMP_H__
#define ___SETJMP_H__

typedef long jmp_buf[20];

/* XXX Need to define sigjmp_buf as well */

#endif /* ___SETJMP_H__ */
