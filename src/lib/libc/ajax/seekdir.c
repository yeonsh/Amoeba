/*	@(#)seekdir.c	1.3	94/04/07 10:31:28 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* seekdir(2) library call replacement */
/* TO DO: get/set dp->offset directly (needs change in dir_open interface) */

#include "ajax.h"
#include <sys/types.h>
#include <dirent.h>

void
seekdir(dirp, loc)
	DIR *dirp;
	long loc;
{
	if (loc < dirp->_dd_loc) {
		dir_rewind((struct dir_open *)dirp->_dd_buf);
		dirp->_dd_loc = 0;
	}
	while (dirp->_dd_loc < loc) {
		if (dir_next((struct dir_open *)dirp->_dd_buf) == NULL)
			break;
		dirp->_dd_loc++;
	}
}
