/*	@(#)tcdrain.c	1.2	94/04/07 10:32:19 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix tcdrain function */

#include "ajax.h"
#include "fdstuff.h"
#include "posix/termios.h"
#include "class/tios.h"

tcdrain(fildes)
	int fildes;
{
	struct fd *f;
	int err;
	
	FDINIT();
	FDLOCK(fildes, f);
	err = tios_drain(&f->fd_cap);
	FDUNLOCK(f);
	if (err != STD_OK) {
		ERR(ENOTTY, "tcdrain: tios_drain error");
	}
	else
		return 0;
}
