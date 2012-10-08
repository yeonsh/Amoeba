/*	@(#)cleanup.s	1.5	96/02/27 11:22:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "syscall_stub.h"

SYSCALL_PRELUDE
_SENTRY(sys_cleanup)
	INT	(CONST(55))
_SEXIT()
SYSCALL_POSTLUDE
