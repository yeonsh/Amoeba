/*	@(#)tcsendbreak.c	1.2	94/04/07 10:32:57 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix tcsendbreak function */

#include "ajax.h"
#include "fdstuff.h"
#include "posix/termios.h"
#include "class/tios.h"

tcsendbreak(fildes, duration)
	int fildes;
	int duration;
{
	struct fd *f;
	int err;
	
	FDINIT();
	FDLOCK(fildes, f);
	err = tios_sendbrk(&f->fd_cap, duration);
	FDUNLOCK(f);
	if (err != STD_OK) {
		ERR(ENOTTY, "tcsendbreak: tios_sendbreak error");
	}
	else
		return 0;
}
