/*	@(#)mu_trylock.s	1.3	94/04/07 11:28:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "syscall.h"
#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(mu_trylock)
	MOVAL	REGOFFSETTED(sp,4),a0
	CLRL	d0
	MOVEQ	#-1,d1
	CASL	d0,d1,REGINDIR(a0)
	bne	dosyscall
	rts
dosyscall:
	_SCALL(mu_trylock)
	rts
SYSCALL_POSTLUDE
