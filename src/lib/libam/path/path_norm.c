/*	@(#)path_norm.c	1.3	94/04/07 10:09:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Part of the path-processing module */

#include <module/path.h>
#include <string.h>

#ifndef NULL
#define NULL 0
#endif

/*
 * Given an absolute or relative pathname and the name of the current working
 * directory, path_norm builds, in the given character buffer, a normal form
 * for the pathname.  See path.h for details.
 */
int
path_norm(cwd, path, buf, len)
	char *cwd;
	char *path;
	char *buf;
	int len;
{
	register char *from, *to;
	int cwd_len = 0;
	char *end;
	
	if (path == NULL)
		return -1;
	
	from = path;
	to = buf;
	if (cwd)
		cwd_len = strlen(cwd);

	if (*from == '/') {
		*to++ = *from++;
	} else if (cwd) {
		/* Prefix cwd to turn relative path into absolute: */
		if (cwd_len >= len) {
			return -1;
		}
		strcpy(buf, cwd);
		to = buf + cwd_len;
		if (to > buf && to[-1] != '/')
			*to++ = '/';
	}
	
	for (;;) {
		/*
		** Invariant: when we get here, we are at the start of
		** a new component.  If this isn't the first component,
		** a slash has already been appended.
		*/
		
		while (*from == '/')
			from++; /* Skip extra slashes */
		end = from;
		while (*end != '\0' && *end != '/')
			++end;
		if (from[0] == '.') {
			if (end-from == 1) {
				/* Remove '.' */
				if (*end == '\0')
					break;
				from = end+1;
				continue;
			}
			if (end-from == 2 && from[1] == '.') {
				/* Backup '..' */
				if (to == buf) {
					/* Backup through cwd: */
					if (!cwd || cwd_len >= len) {
						return -1;
					}
					strcpy(buf, cwd);
					to = buf + cwd_len;
					if (to == buf) {
						return -1;
					}
					if (to[-1] != '/')
						*to++ = '/';
				}
				/* if (to[-1] != '/')
					abort(); Can't happen */
				if (to-1 > buf) {
					do {
						to--;
					} while (to > buf && to[-1] != '/');
				}
				if (*end == '\0')
					break;
				from = end+1;
				continue;
			}
		}
		while (from < end)
			*to++ = *from++;
		if (*from == '\0')
			break;
		*to++ = *from++;
	}
	if (to - buf > 1 && to[-1] == '/')
		--to; /* Final touch: strip trailing slash */
	*to = '\0';
	return 0;
}

/*
 * Just like path_norm, except that it only uses cwd to turn a relative
 * path into an absolute path if necessary, i.e., if the path contains enough
 * ".." components to require looking at the current working directory name.
 */
int
path_rnorm(cwd, path, buf, len)
	char *cwd;
	char *path;
	char *buf;
	int len;
{
	int err;

	if (path[0] == '.' && path[1] == '.' && (!path[2] || path[2] == '/'))
		/* If relative path name starting with "..", we definitely
		 * need to use cwd to obtain absolute path name: */
		err = path_norm(cwd, path, buf, len);
	else
		/* Otherwise, cwd is probably irrelevant: */
		if ((err = path_norm((char *)0, path, buf, len)) != 0)
			/* Oops, maybe we needed cwd after all: */
			err = path_norm(cwd, path, buf, len);
	return err;
}

