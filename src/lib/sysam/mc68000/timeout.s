/*	@(#)timeout.s	1.3	96/02/27 11:28:28 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(timeout)

	MOVL	REGOFFSETTED(sp,4),d0
	MOVL    GLOBAL(__thread_local),AUTODEC(sp)
	trap	#10
	MOVL    AUTOINC(sp),GLOBAL(__thread_local)
	rts

SYSCALL_POSTLUDE
