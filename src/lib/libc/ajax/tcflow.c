/*	@(#)tcflow.c	1.2	94/04/07 10:32:26 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix tcflow function */

#include "ajax.h"
#include "fdstuff.h"
#include "posix/termios.h"
#include "class/tios.h"

tcflow(fildes, action)
	int fildes;
	int action;
{
	struct fd *f;
	int err;
	
	FDINIT();
	FDLOCK(fildes, f);
	err = tios_flow(&f->fd_cap, action);
	FDUNLOCK(f);
	if (err != STD_OK) {
		ERR(ENOTTY, "tcflow: tios_flow error");
	}
	else
		return 0;
}
