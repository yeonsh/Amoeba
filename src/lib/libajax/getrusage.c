/*	@(#)getrusage.c	1.3	94/04/07 09:47:18 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* getrusage(2) system call emulation */
/* TO DO: DO IT */

#include "ajax.h"
#include <sys/time.h>
#include <sys/resource.h>

int
getrusage(who, rusage)
	int who;
	struct rusage *rusage;
{
	if (rusage != NULL)
		(void) memset((_VOIDSTAR) rusage, 0, sizeof *rusage);
	return 0;
}
