/*	@(#)opendir.c	1.4	94/04/07 10:30:31 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* opendir(3) POSIX 5.1.2 */

#include "ajax.h"
#include <sys/types.h>
#include <dirent.h>

DIR *
opendir(name)
	char *name;
{
	capability dircap;
	struct dir_open *dp;
	DIR *dirp;
	
	if (name_lookup(name, &dircap) != 0)
		ERR2(ENOENT, "opendir: directory not found", NULL);
	if ((dirp = (DIR *)malloc(sizeof(DIR))) == NULL)
		ERR2(ENOMEM, "opendir: no mem for DIR *", NULL);
	if ((dp = dir_open(&dircap)) == NULL) {
		free((_VOIDSTAR) dirp);
		ERR2(ENOTDIR, "opendir: dir_open failed", NULL);
	}
	dirp->_dd_magic = __DIR_MAGIC;
	dirp->_dd_loc = 0;
	dirp->_dd_buf = (char *)dp;
	return dirp;
}
