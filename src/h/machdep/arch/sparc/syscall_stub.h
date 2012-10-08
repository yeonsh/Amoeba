/*	@(#)syscall_stub.h	1.3	96/02/27 10:30:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SYSCALL_STUB_H__
#define __SYSCALL_STUB_H__

#include "assyntax.h"
#include "fault.h"

/*
** The macro's in this file make it possible to have stubs
** independent of machine.
*/

#ifdef __STDC__
#define _CAT(a,b) a##b
#else
#define _CAT(a,b) a/**/b
#endif


#ifdef __STDC__
#define	_SENTRY(name)	GLOBNAME(_##name)
#else
#define	_SENTRY(name)	GLOBNAME(_/**/name)
#endif

#define _SCALL(name) \
		sethi	%hi( GLNAME(_thread_local) ), %g3		;\
		ld	[ %g3 + %lo( GLNAME(_thread_local) )], %g2;	;\
		mov	_CAT(SC_,name), %g1				;\
		ta	TRAPV_SYSCALL					;\
		st	%g2, [ %g3 + %lo( GLNAME(_thread_local) )];

#define _SEXIT() \
		retl	;\
		nop

#define	SYSCALL_PRELUDE		AS_BEGIN
#define	SYSCALL_POSTLUDE

#define	SYSCALL(name) \
		SYSCALL_PRELUDE		;\
		_SENTRY(name)		;\
		_SCALL(name)		;\
		_SEXIT()		;\
		SYSCALL_POSTLUDE

#endif /* __SYSCALL_STUB_H__ */
