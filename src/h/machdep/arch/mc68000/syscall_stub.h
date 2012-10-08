/*	@(#)syscall_stub.h	1.3	96/02/27 10:29:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SYSCALL_STUB_H__
#define __SYSCALL_STUB_H__

/*
** The macro's in this file make it possible to have stubs
** independent of machine.
*/

#include "assyntax.h"

#ifdef __STDC__
#define _CAT(a,b)	a##b
#else
#define _CAT(a,b)	a/**/b
#endif
#define _SENTRY(name) \
	GLOBLDIR	_CAT(__,name)					;\
	_CAT(__,name):	

#define _SCALL(name) \
		MOVL    GLOBAL(__thread_local),AUTODEC(sp)		;\
		MOVL	#_CAT(SC_,name),AUTODEC(sp)			;\
		trap	#13						;\
		ADDQAL	#4,sp						;\
		MOVL    AUTOINC(sp),GLOBAL(__thread_local)

#define _SEXIT() rts

#define	SYSCALL_PRELUDE	AS_BEGIN
#define	SYSCALL_POSTLUDE

#define SYSCALL(name) \
		SYSCALL_PRELUDE		;\
		_SENTRY(name)		;\
		_SCALL(name)		;\
		_SEXIT()		;\
		SYSCALL_POSTLUDE

#endif /* __SYSCALL_STUB_H__ */
