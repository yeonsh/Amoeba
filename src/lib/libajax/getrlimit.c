/*	@(#)getrlimit.c	1.3	94/04/07 09:47:00 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* getrlimit(2) system call emulation */
/* TO DO: DO IT */

#include "ajax.h"
#include <sys/time.h>
#include <sys/resource.h>

int
getrlimit(who, rlp)
	int who;
	struct rlimit *rlp;
{
	if (rlp != NULL)
		(void) memset((_VOIDSTAR) rlp, 0, sizeof *rlp);
	return 0;
}
