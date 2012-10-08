/*	@(#)path_lookup.c	1.2	94/04/07 10:09:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <stderr.h>
#include <module/name.h>
#include <module/path.h>

#ifndef NULL
#define NULL 0
#endif

/*
 * Used to get the full pathname and capability for the first entry with
 * given filename found in the colon-separated list of directories.  See
 * path.h for details.
 */
char *
path_lookup(dirlist, filename, path_buff, cap_ptr)
	char *dirlist, *filename, *path_buff;
	capability *cap_ptr;
{
	while (dirlist != NULL) {
		dirlist = path_first(dirlist, filename, path_buff);
		if (name_lookup(path_buff, cap_ptr) == STD_OK)
			return dirlist;
	}
	path_buff[0] = '\0';
	return dirlist;
}
