/*	@(#)trans.s	1.3	96/02/27 11:28:35 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(trans)

	link	a6,#0
#ifdef FROZEN_SUPPORT
	MOVL	GLOBAL(_am_frozen),AUTODEC(sp)
#endif
	MOVEML	d2-d7/a2-a5,AUTODEC(sp)	COM save registers d2-d7/a2-a5 (0x3F3C)
	MOVAL	REGOFFSETTED(a6,8),a0	COM get pointer to header)
	MOVEML	REGINDIR(a0),d3-d7/a1-a3 COM hdr1 in d3-d7/a1-a3 (0xEF8)
	MOVAL	REGOFFSETTED(a6,12),a4	COM buf1 in a4)
	MOVL	REGOFFSETTED(a6,16),d1	COM cnt1 in d1)
	MOVAL	REGOFFSETTED(a6,24),a5	COM buf2 in a5)
	MOVL	REGOFFSETTED(a6,28),d2	COM cnt2 in d2)
	MOVL	GLOBAL(__thread_local),AUTODEC(sp)
	trap	#6			COM call process task
	MOVL	AUTOINC(sp),GLOBAL(__thread_local)
	MOVAL	REGOFFSETTED(a6,20),a0	COM get pointer to header)
	MOVEML	d3-d7/a1-a3,REGINDIR(a0)COM get reply header (0xEF8)
	MOVEML	AUTOINC(sp),d2-d7/a2-a5	COM restore registers d2-d7/a2-a5 (0x3CFC)
#ifdef FROZEN_SUPPORT
	TSTL	GLOBAL(_am_frozen)
	bne	L1
	TSTL	AUTOINC(sp)
	bra	L2

L1:
	TSTL	AUTOINC(sp)
	bne	L2
	MOVL	d0,AUTODEC(sp)
	jsr	GLOBAL(_am_sched)
	MOVL	AUTOINC(sp),d0
L2:
#endif
	unlk	a6
	rts

SYSCALL_POSTLUDE
