/*	@(#)path_first.c	1.2	94/04/07 10:09:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include <module/path.h>

#ifndef NULL
#define NULL 0
#endif

/*
 * Used to search in a colon-separated list of directories for a given
 * filename.   See path.h for details
 */
char *
path_first(dirlist, filename, path_buff)
	char *dirlist, *filename, *path_buff;
{
	register char *dirend, *rtn;
	register int dirlen;

	if (dirlist == NULL) return NULL;
	for (dirend = dirlist; *dirend != '\0' && *dirend != ':'; dirend++)
		/* no-op */ ;
	dirlen = dirend - dirlist;
	if (dirlen == 0 || *filename == '/')
		/* The first dir is an empty string or file is absolute: */
		(void)strcpy(path_buff, filename);
	else {
		(void)strncpy(path_buff, dirlist, dirlen);
		path_buff[dirlen] = '/';
		(void)strcpy(path_buff + dirlen + 1, filename);
	}
	rtn = *dirend == '\0' || *filename == '/' ? NULL : dirend + 1;
#ifdef DEBUG
	printf("path_first(%s, %s, !%s) -> %s\n", dirlist, filename,
						  path_buff, rtn);
#endif
	return rtn;
}
