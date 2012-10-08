/*	@(#)getcwd.c	1.4	96/02/27 11:05:39 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix-conformant getcwd() function */

#include "ajax.h"

char *
getcwd(buf, len)
	char *buf;
	int len;
{
	char *workpath;
	capability cap;
	
	if (buf == NULL || len <= 0) {
		errno = EINVAL;
		return NULL;
	}
	if ((workpath = getenv("_WORK")) != NULL && *workpath == '/') {
		if (strlen(workpath) >= len) {
			errno = ERANGE;
			return NULL;
		}
	}
	else
		workpath = "/";
	if (name_lookup(workpath, &cap) != STD_OK ||
	    memcmp((_VOIDSTAR) &cap, (_VOIDSTAR) dir_origin(""), CAPSIZE) != 0) {
		errno = EACCES;
		return NULL;
	}
	return strcpy(buf, workpath);
}
