/*	@(#)getpriority.s	1.3	96/02/27 11:23:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "syscall.h"
#include "syscall_stub.h"

SYSCALL_1(thread_get_max_prio)
