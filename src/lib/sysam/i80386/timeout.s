/*	@(#)timeout.s	1.5	96/02/27 11:25:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(timeout)
	MOV_L	(REGOFF(8,EBP),EBX)
	INT	(CONST(54))
_SEXIT()
SYSCALL_POSTLUDE
