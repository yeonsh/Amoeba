/*	@(#)mkdir.c	1.4	96/02/27 11:05:43 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix mkdir() function */

#include "ajax.h"
#include <sys/types.h>
#include <sys/stat.h>


#ifdef __STDC__
int mkdir(char *path, mode_t mode)
#else
int
mkdir(path, mode)
	char *path;
	mode_t mode;
#endif
{
	errstat err;
	if ((err = sp_mkdir(SP_DEFAULT, path, (char **)NULL)) != STD_OK)
		ERR(_ajax_error(err), "mkdir: sp_mkdir failed");
	return 0;
}
