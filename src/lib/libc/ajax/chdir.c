/*	@(#)chdir.c	1.3	94/04/07 10:24:13 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix chdir() function */

#include "ajax.h"

int
chdir(path)
	char *path;
{
	capability cap;
	
	if (name_lookup(path, &cap) != STD_OK) {
		ERR(ENOENT, "chdir: lookup err");
	}
	if (!_is_am_dir(&cap)) {
		ERR(ENOTDIR, "chdir: not a directory");
	}
	if (cwd_set(path) != STD_OK) {
		ERR(EACCES, "chdir: cwd_set failed");
	}
	return 0;
}
