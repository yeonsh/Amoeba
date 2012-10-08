/*	@(#)getpriority.c	1.2	94/04/07 09:45:41 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* getpriority(2) system call emulation */
/* TO DO: DO IT */

#include "ajax.h"

#include <sys/time.h>
#include <sys/resource.h>

int
getpriority(which, who)
	int which;
	int who;
{
	return 0; /* Fraternite, liberte, egalite */
}
