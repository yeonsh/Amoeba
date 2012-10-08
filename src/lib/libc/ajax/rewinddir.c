/*	@(#)rewinddir.c	1.3	94/04/07 10:31:14 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* rewinddir() POSIX 5.1.2 
 * last modified apr 22 93 Ron Visser
 */

#include <ajax.h>
#include <sys/types.h>
#include <dirent.h>
#include <sp_dir.h>

void
rewinddir(dd)
DIR *dd;
{
/* According to POSIX the rewinddir function should cause the
 * directory stream to refer to the current state of the directory as a
 * a call to opendir would. To achieve this we reopen the directory.
 */
  capability dircap;
  struct dir_open *dp;

  if(dd == NULL || dd->_dd_magic != __DIR_MAGIC) {
	return; /* nothing specified about errno in this case */
  }

  cs_to_cap(&(((SP_DIR *)dd->_dd_buf)->dd_capset),&dircap);

  (void) dir_close((struct dir_open *)dd->_dd_buf);

  if ((dp = dir_open(&dircap)) == NULL) {
	dd->_dd_magic = 0L; /* invalidate */
	free((_VOIDSTAR) dd);
	return; /* nothing specified about errno in this case */
  }
  dd->_dd_loc = 0;
  dd->_dd_buf = (char *)dp;
}
