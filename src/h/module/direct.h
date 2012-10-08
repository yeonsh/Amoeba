/*	@(#)direct.h	1.4	96/02/27 10:32:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_DIRECT_H__
#define __MODULE_DIRECT_H__

#include <server/direct/direct.h>	/* For DIR_BUFFER */

/* Structure returned by dir_open() */
struct dir_open {
	capability dircap;
	char *next, *last;
	long offset;
	char buf[DIR_BUFFER];
};

#define	dir_origin	_dir_origin
#define	dir_append	_dir_append
#define	dir_breakpath	_dir_breakpath
#define	dir_create	_dir_create
#define	dir_delete	_dir_delete
#define	dir_lookup	_dir_lookup
#define	dir_replace	_dir_replace

#define	dir_open	_dir_open
#define	dir_next	_dir_next
#define	dir_close	_dir_close
#define	dir_rewind	_dir_rewind

/* function prototypes for directory server */
capability *	dir_origin _ARGS((const char *));
errstat		dir_append _ARGS((capability *, char *, capability *));
char *		dir_breakpath _ARGS((capability *, char *, capability *));
errstat		dir_create _ARGS((capability *, capability *));
errstat		dir_delete _ARGS((capability *, char *));
errstat		dir_lookup _ARGS((capability *, char *, capability *));
errstat		dir_replace _ARGS((capability *, char *, capability *));

struct dir_open *dir_open _ARGS((capability *));
char *		dir_next _ARGS((struct dir_open *));
errstat		dir_close _ARGS((struct dir_open *));
errstat		dir_rewind _ARGS((struct dir_open *));

#endif /* __MODULE_DIRECT_H__ */
