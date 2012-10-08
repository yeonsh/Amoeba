/*	@(#)getpgrp.c	1.3	94/04/07 09:44:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* getpgrp() POSIX 4.3.1
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sesstuff.h"

pid_t
getpgrp()
{
	capability *session;
	int pgrp;
	
	SESINIT(session);
	if (ses_getpgrp(session, 0, &pgrp) < 0)
		ERR(EIO, "getpgrp: failed");
	return pgrp;
}
