/*	@(#)tcgetwsize.c	1.2	94/04/07 10:32:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix tcgetwsize extention */

#include "ajax.h"
#include "fdstuff.h"
#include "posix/sys/ioctl.h"
#include "class/tios.h"

tcgetwsize(fildes, wp)
	int fildes;
	struct winsize *wp;
{
	struct fd *f;
	int err;
	int width, length;
	
	FDINIT();
	FDLOCK(fildes, f);
	err = tios_getwsize(&f->fd_cap, &width, &length);
	FDUNLOCK(f);
	if (err != STD_OK) {
		ERR(ENOTTY, "tcgetwsize: tios_getwsize error");
		/*NOTREACHED*/
	}
	else {
		/* Dummy values; can't find real info */
		wp->ws_xpixel = wp->ws_ypixel = 0;
		wp->ws_col = width;
		wp->ws_row = length;
		return 0;
	}
	/*NOTREACHED*/
}
