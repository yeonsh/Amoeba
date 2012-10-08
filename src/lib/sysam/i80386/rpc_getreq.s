/*	@(#)rpc_getreq.s	1.6	96/02/27 11:24:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(rpc_getreq)
	MOV_L	(REGOFF(8,EBP),EDI)	/* hdr */
	MOV_L	(REGOFF(12,EBP),ESI)	/* buf */
	MOV_L	(REGOFF(16,EBP),EBX)	/* cnt */
	INT	(CONST(58))
_SEXIT()
SYSCALL_POSTLUDE
