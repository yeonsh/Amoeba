/*	@(#)readdir.c	1.3	94/04/07 10:31:01 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* readdir() POSIX 5.1.2 
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include <sys/types.h>
#include <dirent.h>

struct dirent *
readdir(dirp)
DIR *dirp;
{
  struct dirent *d;
  char *p;
	
  if(dirp == NULL || dirp->_dd_magic != __DIR_MAGIC)
	ERR2(EBADF,"readdir: bad filedescriptor", NULL);

  if ((p = dir_next((struct dir_open *)dirp->_dd_buf)) == NULL)
	return NULL;

  dirp->_dd_loc++;
  d = &dirp->_dd_ent;
  strncpy(d->d_name, p, sizeof d->d_name);
  d->d_name[sizeof d->d_name - 1] = '\0';
  return d;
}
