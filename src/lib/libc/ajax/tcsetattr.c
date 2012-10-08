/*	@(#)tcsetattr.c	1.2	94/04/07 10:33:06 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix tcsetattr function */

#include "ajax.h"
#include "fdstuff.h"
#include "posix/termios.h"
#include "class/tios.h"

tcsetattr(fildes, optional_actions, tp)
	int fildes;
	int optional_actions;
	struct termios *tp;
{
	struct fd *f;
	int err;
	
	FDINIT();
	FDLOCK(fildes, f);
	err = tios_setattr(&f->fd_cap, optional_actions, tp);
	FDUNLOCK(f);
	if (err != STD_OK) {
		ERR(ENOTTY, "tcsetattr: tios_setattr error");
	}
	else
		return 0;
}
