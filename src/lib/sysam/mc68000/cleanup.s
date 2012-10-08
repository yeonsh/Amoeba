/*	@(#)cleanup.s	1.4	96/02/27 11:25:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(sys_cleanup)

#ifdef FROZEN_SUPPORT
	MOVL	GLOBAL(_am_frozen),AUTODEC(sp)
#endif
	MOVL	GLOBAL(__thread_local),AUTODEC(sp)
	trap	#11
	MOVL	AUTOINC(sp),GLOBAL(__thread_local)
#ifdef FROZEN_SUPPORT
	TSTL	GLOBAL(_am_frozen)
	bne	L1
	TSTL	AUTOINC(sp)
	rts

L1:
	TSTL	AUTOINC(sp)
	bne	L2
	MOVL	d0,AUTODEC(sp)
	jsr	GLOBAL(_am_sched)
	MOVL	AUTOINC(sp),d0
L2:
#endif
	rts

SYSCALL_POSTLUDE
