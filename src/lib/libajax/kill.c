/*	@(#)kill.c	1.3	94/04/07 09:47:50 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* kill() POSIX 3.3.2
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sesstuff.h"

int
kill(pid, sig)
	pid_t pid;
	int   sig;
{
	capability *session;
	int err;
	
	SESINIT(session);
	if ((err = ses_kill(session, pid, sig)) < 0) {
		if (err == STD_NOTFOUND) {
			ERR(ESRCH, "kill: no such process");
		}
		else {
			ERR(EINVAL, "kill: unknown failure");
		}
	}
	return 0;
}
