/*	@(#)path.h	1.3	96/02/27 10:33:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_PATH_H__
#define __MODULE_PATH_H__


		/* A Path Analysis and Processing Module */
/*
 * A collection of functions for analysing path names and colon-separated
 * directory lists such as the PATH environment variable.  This module
 * implements the semantics of "." and ".." as path components, by doing
 * syntactic evaluation of such components in the user program.  The directory
 * server knows nothing about "." or "..".
 */

#define	path_first	_path_first
#define	path_lookup	_path_lookup
#define	path_norm	_path_norm
#define	path_rnorm	_path_rnorm
#define	path_capnorm	_path_capnorm
#define	path_cs_norm	_path_cs_norm

extern char *path_first(/* char *dirlist, char *filename, char *path_buff */);
extern char *path_lookup(/* char *dirlist, char *filename, char *path_buff,
							capability *cap_ptr */);
extern int path_norm(/* char *cwd, char *path, char *buf, int len */);
extern int path_rnorm(/* char *cwd, char *path, char *buf, int len */);
extern int path_capnorm(/* capability **dircap_p, char *path, char *buf,
								   int len */);
extern int path_cs_norm(/* capset **dircapset_p, char *path, char *buf,
								   int len */);

#endif /* __MODULE_PATH_H__ */
