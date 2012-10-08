/*	@(#)wait.c	1.3	94/04/07 09:56:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* wait() POSIX 3.2.1
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"

pid_t
wait(pstatus)
	int *pstatus;
{
	return waitpid(-1, pstatus, 0);
}
