/*	@(#)lstat.c	1.2	94/04/07 09:48:46 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* lstat(2) system call emulation */

#include "ajax.h"
#include <sys/types.h>
#include <sys/stat.h>

/* For compatibility with UNIX programs that believe in symbolic links,
 * which Amoeba doesn't have:
 */

int
lstat(path, buf)
	char *path;
	struct stat *buf;
{
	return stat(path, buf);
}
