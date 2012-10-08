/*	@(#)tcgetattr.c	1.2	94/04/07 10:32:46 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix tcgetattr function */

#include "ajax.h"
#include "fdstuff.h"
#include "posix/termios.h"
#include "class/tios.h"

tcgetattr(fildes, tp)
	int fildes;
	struct termios *tp;
{
	struct fd *f;
	int err;
	
	FDINIT();
	FDLOCK(fildes, f);
	err = tios_getattr(&f->fd_cap, tp);
	FDUNLOCK(f);
	if (err != STD_OK) {
		ERR(ENOTTY, "tcgetattr: tios_getattr error");
	}
	else
		return 0;
}
