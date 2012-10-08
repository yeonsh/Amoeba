/*	@(#)dirent.h	1.4	94/04/06 16:52:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Posix 1003.1 compliant definitions for opendir() c.s. */

#ifndef __DIRENT_H__
#define __DIRENT_H__

#include "_ARGS.h"

struct dirent {
	/* BSD's d_ino etc. are not included since they are not required
	 * by Posix, and would not be useful under Amoeba anyway.
	 */
	char d_name[256];
};

typedef struct {
	/* The contents of this structure is not defined by Posix */
	long	      _dd_magic;	/* magic cookie */
	long	      _dd_loc;		/* for seekdir (non Posix) */
	struct dirent _dd_ent;		/* buffer for readdir() */
	char *	      _dd_buf;		/* buffer for direct module */
} DIR;

DIR *opendir _ARGS((char *_dirname));
struct dirent *readdir _ARGS((DIR *_dirp));
void rewinddir _ARGS((DIR *_dirp));
int closedir _ARGS((DIR *_dirp));

/* internal use only */
#define __DIR_MAGIC	0xFAFAFAFA

#endif /* __DIRENT_H__ */
