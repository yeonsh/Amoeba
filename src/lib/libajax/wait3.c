/*	@(#)wait3.c	1.4	94/04/07 09:56:14 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* wait3(2) system call emulation */

#include "amoeba.h"
#include "stderr.h"
#include "string.h"
#include "sesstuff.h"
#include "sigstuff.h"

#undef _POSIX_SOURCE	/* to get union wait typedef */
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

int
wait3(psts, options, rusage)
	union wait *psts;
	int options;
	struct rusage *rusage;
{
	if (rusage != NULL)
		(void) memset((_VOIDSTAR) rusage, 0, sizeof *rusage);
	return waitpid(-1, (int *)psts, options);
}
