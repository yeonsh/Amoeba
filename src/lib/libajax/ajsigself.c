/*	@(#)ajsigself.c	1.2	94/04/07 09:42:14 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Send a signal to myself (hoping the session server will notice) */

#include "ajax.h"

int
_ajax_sigself(sig)
	int sig;
{
	capability self;
	
	if (getinfo(&self, NILPD, 0) < 0)
		ERR(EIO, "sigself: getinfo failed");
	(void) pro_stun(&self, sig);
	return 0;
}
