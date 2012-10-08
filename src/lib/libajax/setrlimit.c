/*	@(#)setrlimit.c	1.2	94/04/07 09:51:20 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* setrlimit(2) system call emulation */
/* TO DO: DO IT */

#include "ajax.h"
#include <sys/time.h>
#include <sys/resource.h>

int
setrlimit(who, rlp)
	int who;
	struct rlimit *rlp;
{
	return 0; /* Pretend it's done */
}
