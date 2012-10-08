/*	@(#)rpc_putrep.s	1.4	96/02/27 11:27:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "syscall_stub.h"
#include "frozen.h"

SYSCALL_PRELUDE
_SENTRY(rpc_putrep)

#ifdef FROZEN_SUPPORT
	MOVL	GLOBAL(_am_frozen),AUTODEC(sp)
#endif
	link	a6,#0
	MOVEML	d3-d7/a2-a5,AUTODEC(sp)		COM save d3-d7/a2-a5 (0x1F3C)
	MOVAL	REGOFFSETTED(a6,OFF_HDR),a0	COM get pointer to hdr
	MOVEML	REGINDIR(a0),d3-d7/a1-a3	COM hdr in d3-d7/a1-a3 (0xEF8)
	MOVAL	REGOFFSETTED(a6,OFF_BUF),a4	COM buf in a4
	MOVL	REGOFFSETTED(a6,OFF_CNT),d1	COM cnt in d1
	MOVL	GLOBAL(__thread_local),AUTODEC(sp)
	trap	#3				COM call process task
	MOVL	AUTOINC(sp),GLOBAL(__thread_local)
	MOVEML	AUTOINC(sp),d3-d7/a2-a5		COM restore d3-d7/a2-a5(0x3CF8)
	unlk	a6
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
