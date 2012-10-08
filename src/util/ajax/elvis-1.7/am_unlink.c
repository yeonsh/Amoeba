/*	@(#)am_unlink.c	1.1	96/02/27 12:45:03 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * A special version of unlink for elvis' temporary files.
 * This version not only removes the directory entry, it also destroys
 * the file.  In this way, the temporary loss of disk space on the bullet
 * server is minimized.
 *
 * Author: Gregory J. Sharp, Nov, 1995
 */

#include "ajax/ajax.h"
#include <sys/types.h>
#include <sys/stat.h>


int
unlink(file)
const char * file;
{
    capability dircap;
    capability filecap;
    int mode;

    file = _ajax_breakpath(NILCAP, file, &dircap);
    if (file == NULL)
    {
	ERR(ENOENT, "unlink: bad path");
    }
    if (_ajax_lookup(&dircap, file, &filecap, &mode, (long *) NULL) != STD_OK)
    {
	ERR(ENOENT, "unlink: file not found");
    }
    if ((mode & _S_IFMT) == _S_IFDIR || _is_am_dir(&filecap))
    {
	ERR(EPERM, "unlink: is a directory");
    }
    if (dir_delete(&dircap, (char *) file) != STD_OK)
    {
	ERR(EACCES, "unlink: file not deleted");
    }
    if (std_destroy(&filecap) != STD_OK)
    {
	 ERR(EPERM, "unlink: could not destroy file");
    }
    return 0;
}
