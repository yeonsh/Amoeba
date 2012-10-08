/*	@(#)mu_trylock.s	1.6	96/02/27 11:23:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "syscall.h"
#include "syscall_stub.h"

SYSCALL_2(sys_mu_trylock)
