/*	@(#)mu_unlock.s	1.3	94/04/07 11:28:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "syscall.h"
#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(mu_unlock)
	MOVAL	REGOFFSETTED(sp,4),a0
	CLRL	d0
	MOVEQ	#-1,d1
	CASL	d1,d0,REGINDIR(a0)
	beq	done
	_SCALL(mu_unlock)
done:
	rts
SYSCALL_POSTLUDE
