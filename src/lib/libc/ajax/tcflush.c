/*	@(#)tcflush.c	1.2	94/04/07 10:32:32 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix tcflush function */

#include "ajax.h"
#include "fdstuff.h"
#include "posix/termios.h"
#include "class/tios.h"

tcflush(fildes, queue_selector)
	int fildes;
	int queue_selector;
{
	struct fd *f;
	int err;
	
	FDINIT();
	FDLOCK(fildes, f);
	err = tios_flush(&f->fd_cap, queue_selector);
	FDUNLOCK(f);
	if (err != STD_OK) {
		ERR(ENOTTY, "tcflush: tios_flush error");
	}
	else
		return 0;
}
