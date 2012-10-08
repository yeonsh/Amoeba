/*	@(#)exitthread.s	1.4	96/02/27 11:25:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(exitthread)

	MOVL	REGOFFSETTED(sp,4),d0
	MOVL    GLOBAL(__thread_local),AUTODEC(sp)
	trap	#7
	/*NOTREACHED*/

SYSCALL_POSTLUDE
