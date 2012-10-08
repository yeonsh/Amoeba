/*	@(#)syscall_stub.h	1.2	94/04/06 15:56:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SYSCALL_STUB_H__
#define __SYSCALL_STUB_H__

/*
** The macro's in this file make it possible to have syscall-stubs
** independent of machine.
*/

#ifdef __STDC__
#define CAT(a,b) a##b
#else
#define CAT(a,b) a/**/b
#endif
#define _SENTRY(name) \
	.globl	CAT(_,name)					;\
	CAT(_,name):	

/*
 * Save _thread_local, trap to kernel giving it SC_name as a parameter
 * and restore _thread_local
 */
#define _SCALL(name)

#define _SEXIT() /* return from subroutine */

#define	SYSCALL_PRELUDE	/* anything your assembler needs */
#define  SYSCALL(name) _SENTRY(name); _SCALL(name); _SEXIT()
#define	SYSCALL_POSTLUDE

#endif /* __SYSCALL_STUB_H__ */
