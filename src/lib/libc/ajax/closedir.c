/*	@(#)closedir.c	1.3	94/04/07 10:24:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* closedir() POSIX 5.1.2 
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include <sys/types.h>
#include <dirent.h>

int
closedir(dirp)
DIR *dirp;
{
  if (dirp == NULL || dirp->_dd_magic != __DIR_MAGIC)
	ERR(EBADF, "closedir: not an open directory");

  (void) dir_close((struct dir_open *)dirp->_dd_buf);
  dirp->_dd_magic = 0L;	/* invalidate */
  free((_VOIDSTAR) dirp);
  return 0;
}
