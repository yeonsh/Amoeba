/*	@(#)setpgid.c	1.2	94/04/07 09:50:27 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* setpgid() POSIX 4.3.3
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sesstuff.h"
#include <sys/types.h>

pid_t
setpgid(pid, pgrp)
	pid_t pid, pgrp;
{
	capability *session;
	
	SESINIT(session);
	if (ses_setpgrp(session, pid, pgrp) < 0)
		ERR(EIO, "setpgid: failed");
	return 0;
}
