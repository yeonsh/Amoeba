/*	@(#)rpc_trans.s	1.6	96/02/27 11:24:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(rpc_trans)
	MOV_L	(REGOFF(8,EBP),ESI)	/* hdr1 */
	MOV_L	(REGOFF(12,EBP),EBX)	/* buf1 */
	MOV_L	(REGOFF(16,EBP),ECX)	/* cnt1 */
	MOV_L	(REGOFF(20,EBP),EDI)	/* hdr2 */
	MOV_L	(REGOFF(24,EBP),EDX)	/* buf2 */
	MOV_L	(REGOFF(28,EBP),EBP)	/* cnt2 */
	INT	(CONST(60))
_SEXIT()
SYSCALL_POSTLUDE
