/*	@(#)ajorigin.c	1.4	96/02/27 11:05:10 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Return the proper directory to use as a starting point for path lookup.
   The rules are:
   - if path begins with a '/', use the root directory (getcap("ROOT"))
   - otherwise, if dir is NULL, use the working directory (getcap("WORK"))
   - otherwise, use dir
   (This is equivalent to dir_origin(path) only if dir is NULL.) */

#include "ajax.h"

capability *
_ajax_origin(dir, path)
	capability *dir;
	const char *path;
{
	if (dir == NULL || path[0] == '/')
		return dir_origin(path);
	else
		return dir;
}

/* Same, for capset interface */

errstat
_ajax_csorigin(dir, path, pcs)
	capability *dir;
	const char *path;
	capset *pcs;
{
	if (*path == '/')
		return sp_get_rootdir(pcs);
	else if (dir == NULL)
		return sp_get_workdir(pcs);
	else {
		if (!cs_singleton(pcs, dir))
			return STD_NOMEM;
		else
			return STD_OK;
	}
}
