/*	@(#)syscall_stub.h	1.7	96/02/27 10:29:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SYSCALL_STUB_H__
#define __SYSCALL_STUB_H__

/*
 * The macro's in this file make it possible to have stubs
 * independent of machine type.
 */

#include "assyntax.h"

#ifdef __STDC__
#define _CAT(a,b)	a##b
#else
#define _CAT(a,b)	a/**/b
#endif


#define __SENTRY(name) \
		GLOBL	GLNAME(name)		;\
	GLNAME(name):				;\
		PUSH_L	(EBP)			;\
		MOV_L	(ESP,EBP)		;\
		PUSH_L	(EBX)			;\
		PUSH_L	(ECX)			;\
		PUSH_L	(EDX)			;\
		PUSH_L	(EDI)			;\
		PUSH_L	(ESI)			;\
		PUSH_L	(CONTENT(GLNAME(_thread_local)))

/*
 * We can't call _CAT here because the preprocessor evaluates things in
 * the wrong order(TM).
 */
#ifdef _STDC_
#define	_SENTRY(name) __SENTRY(_##name)
#else
#define	_SENTRY(name) __SENTRY(_/**/name)
#endif

#define	_SCALL0(name) \
		MOV_L	(CONST(_CAT(SC_,name)),ESI) ;\
		INT	(CONST(57))

#define _SCALL1(name) \
		XOR_L	(EDI,EDI)		;\
		XOR_L	(EDX,EDX)		;\
		XOR_L	(ECX,ECX)		;\
		MOV_L	(REGOFF(8,EBP),EBX)	;\
		_SCALL0(name)

#define _SCALL2(name) \
		XOR_L	(EDI,EDI)		;\
		XOR_L	(EDX,EDX)		;\
		MOV_L	(REGOFF(12,EBP),ECX)	;\
		MOV_L	(REGOFF(8,EBP),EBX)	;\
		_SCALL0(name)

#define _SCALL3(name) \
		XOR_L	(EDI,EDI)		;\
		MOV_L	(REGOFF(16,EBP),EDX)	;\
		MOV_L	(REGOFF(12,EBP),ECX)	;\
		MOV_L	(REGOFF(8,EBP),EBX)	;\
		_SCALL0(name)

#define _SCALL4(name) \
		MOV_L	(REGOFF(20,EBP),EDI)	;\
		MOV_L	(REGOFF(16,EBP),EDX)	;\
		MOV_L	(REGOFF(12,EBP),ECX)	;\
		MOV_L	(REGOFF(8,EBP),EBX)	;\
		_SCALL0(name)

#define _SCALL5(name) \
		PUSH_L	(CONST(0))		;\
		PUSH_L	(REGOFF(24,EBP))	;\
		PUSH_L	(REGOFF(20,EBP))	;\
		MOV_L	(ESP,EDI)		;\
		MOV_L	(REGOFF(16,EBP),EDX)	;\
		MOV_L	(REGOFF(12,EBP),ECX)	;\
		MOV_L	(REGOFF(8,EBP),EBX)	;\
		_SCALL0(name)			;\
		ADD_L	(CONST(12),ESP)

#define _SCALL6(name) \
		PUSH_L	(REGOFF(28,EBP))	;\
		PUSH_L	(REGOFF(24,EBP))	;\
		PUSH_L	(REGOFF(20,EBP))	;\
		MOV_L	(ESP,EDI)		;\
		MOV_L	(REGOFF(16,EBP),EDX)	;\
		MOV_L	(REGOFF(12,EBP),ECX)	;\
		MOV_L	(REGOFF(8,EBP),EBX)	;\
		_SCALL0(name)			;\
		ADD_L	(CONST(12),ESP)

#define _SEXIT() \
		POP_L	(CONTENT(GLNAME(_thread_local))) ;\
		POP_L	(ESI)			;\
		POP_L	(EDI)			;\
		POP_L	(EDX)			;\
		POP_L	(ECX)			;\
		POP_L	(EBX)			;\
		POP_L	(EBP)			;\
		RET

#define	SYSCALL_PRELUDE		AS_BEGIN ; SEG_TEXT
#define	SYSCALL_POSTLUDE


#define SYSCALL_0(name) \
    SYSCALL_PRELUDE; _SENTRY(name); _SCALL0(name); _SEXIT(); SYSCALL_POSTLUDE
#define SYSCALL_1(name) \
    SYSCALL_PRELUDE; _SENTRY(name); _SCALL1(name); _SEXIT(); SYSCALL_POSTLUDE
#define SYSCALL_2(name) \
    SYSCALL_PRELUDE; _SENTRY(name); _SCALL2(name); _SEXIT(); SYSCALL_POSTLUDE
#define SYSCALL_3(name) \
    SYSCALL_PRELUDE; _SENTRY(name); _SCALL3(name); _SEXIT(); SYSCALL_POSTLUDE
#define SYSCALL_4(name) \
    SYSCALL_PRELUDE; _SENTRY(name); _SCALL4(name); _SEXIT(); SYSCALL_POSTLUDE
#define SYSCALL_5(name) \
    SYSCALL_PRELUDE; _SENTRY(name); _SCALL5(name); _SEXIT(); SYSCALL_POSTLUDE
#define SYSCALL_6(name) \
    SYSCALL_PRELUDE; _SENTRY(name); _SCALL6(name); _SEXIT(); SYSCALL_POSTLUDE

#endif /* __SYSCALL_STUB_H__ */
