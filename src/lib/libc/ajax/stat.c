/*	@(#)stat.c	1.3	94/04/07 10:31:53 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix stat() function for Amoeba */

#include "ajax.h"
#include <sys/types.h>
#include <sys/stat.h>

int
stat(path, buf)
	char *path;
	struct stat *buf;
{
	capability cap;
	int mode;
	long mtime;
	
	if (_ajax_lookup(NILCAP, path, &cap, &mode, &mtime) != STD_OK)
		ERR(ENOENT, "stat: path not found");
	if (_ajax_capstat(&cap, buf, mode, -1L) < 0)
		return -1;
	buf->st_mtime = mtime;
	/* atime and ctime are left zero, to show they are invalid */
	return 0;
}
