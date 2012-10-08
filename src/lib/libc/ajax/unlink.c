/*	@(#)unlink.c	1.5	94/04/07 10:33:37 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix unlink() function */

#include "ajax.h"
#include <sys/types.h>
#include <sys/stat.h>

int
unlink(file)
	const char *file;
{
	return _ajax_unlink(NILCAP, file);
}

int
_ajax_unlink(pdir, file)
	capability *pdir;
	const char *file;
{
	capability dircap, cap;
	int mode;
	
	file = _ajax_breakpath(pdir, file, &dircap);
	if (file == NULL)
		ERR(ENOENT, "unlink: bad path");
	if (_ajax_lookup(&dircap, file, &cap, &mode, (long *) NULL) != STD_OK)
		ERR(ENOENT, "unlink: file not found");
	if ((mode & _S_IFMT) == _S_IFDIR || _is_am_dir(&cap))
		ERR(EPERM, "unlink: is a directory");
	if (dir_delete(&dircap, (char *) file) != STD_OK)
		ERR(EACCES, "unlink: file not deleted");
	return 0;
}
