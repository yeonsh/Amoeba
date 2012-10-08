/*	@(#)utime.c	1.4	94/04/07 09:55:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* utime() POSIX 5.6.6 
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include <sys/types.h>
#include <utime.h>

/* This is just a high-tech dummy. It returns an error code when an
 * error occurs and ENOSYS when the operation would have been successful
 * because it is not possible to modify the time stored by soap.
 */
int
utime(path, times)
	char *path;
	struct utimbuf *times;
{
	capability dircap;
	
	path = _ajax_breakpath(NILCAP, path, &dircap);
	if (path == NULL)
		ERR(ENOENT, "utime: bad path");
	if (_ajax_lookup(&dircap, path, NILCAP, (int *)0, (long *)0) != STD_OK)
		ERR(ENOENT, "utime: path not found");
	ERR(ENOSYS, "utime: not supported");
}
