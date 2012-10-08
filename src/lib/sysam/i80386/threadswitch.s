/*	@(#)threadswitch.s	1.5	96/02/27 11:25:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "syscall_stub.h"

SYSCALL_PRELUDE

/* this calls await in the kernel (see as.S) */
_SENTRY(threadswitch)
	MOV_L	(ADDR(GLNAME(_threadswitch)),EBX)
	MOV_L	(CONST(-1),ECX)
	INT	(CONST(56))
_SEXIT()
SYSCALL_POSTLUDE
