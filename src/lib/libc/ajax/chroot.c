/*	@(#)chroot.c	1.3	94/04/07 10:24:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* chroot() function (not in Posix) */

#include "ajax.h"

int
chroot(path)
	char *path;
{
	capability *cap;
	capability tmp;
	
	if ((cap = getcap("ROOT")) == NILCAP)
		ERR(EIO, "chroot: no ROOT capability in environment");
	if (name_lookup(path, &tmp) != STD_OK)
		ERR(ENOENT, "chroot: path not found");
	if (!_is_am_dir(&tmp))
		ERR(ENOTDIR, "chroot: not a directory");
	*cap = tmp;
	return 0;
}
