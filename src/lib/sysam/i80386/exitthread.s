/*	@(#)exitthread.s	1.5	96/02/27 11:22:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "syscall_stub.h"

SYSCALL_PRELUDE

/* This calls usuicide in the kernel (see as.S) */
_SENTRY(exitthread)
	MOV_L	(REGOFF(8,EBP), EAX)
	INT	(CONST(53))
_SEXIT()
SYSCALL_POSTLUDE
