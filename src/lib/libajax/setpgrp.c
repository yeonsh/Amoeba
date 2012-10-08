/*	@(#)setpgrp.c	1.2	94/04/07 09:50:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* setpgrp(2) system call emulation */

#include "ajax.h"
#include "sesstuff.h"

int
setpgrp(pid, pgrp)
	int pid, pgrp;
{
	capability *session;
	
	SESINIT(session);
	if (ses_setpgrp(session, pid, pgrp) < 0)
		ERR(EIO, "setpgrp: failed");
	return 0;
}
