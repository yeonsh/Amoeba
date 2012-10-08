/*	@(#)putrep.s	1.5	96/02/27 11:24:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(putrep)
	MOV_L	(REGOFF(8,EBP),ESI)
	MOV_L	(REGOFF(12,EBP),ECX)
	MOV_L	(REGOFF(16,EBP),EBX)
	INT	(CONST(51))
_SEXIT()
SYSCALL_POSTLUDE
