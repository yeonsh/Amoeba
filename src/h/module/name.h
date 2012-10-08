/*	@(#)name.h	1.4	96/02/27 10:33:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_NAME_H__
#define __MODULE_NAME_H__

#define	name_append	_name_append
#define	name_breakpath	_name_breakpath
#define	cwd_set		_cwd_set
#define	name_create	_name_create
#define	name_delete	_name_delete
#define	name_lookup	_name_lookup
#define	name_remove	_name_remove
#define	name_replace	_name_replace

errstat		name_append _ARGS((char *, capability *));
char *		name_breakpath _ARGS((char *, capability *));
errstat		cwd_set _ARGS((char *));
errstat		name_create _ARGS((char *));
errstat		name_delete _ARGS((char *));
errstat		name_lookup _ARGS((char *, capability *));
errstat		name_remove _ARGS((char *));
errstat		name_replace _ARGS((char *, capability *));

#endif /* __MODULE_NAME_H__ */
