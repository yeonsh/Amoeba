/*	@(#)telldir.c	1.3	94/04/07 09:54:26 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* telldir(2) library call replacement */

#include "ajax.h"
#include <sys/types.h>
#include <dirent.h>

long
telldir(dirp)
	DIR *dirp;
{
	return dirp->_dd_loc;
}
