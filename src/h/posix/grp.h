/*	@(#)grp.h	1.3	94/04/06 16:53:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __GRP_H__
#define __GRP_H__

/* Posix 1003.1 compliant definitions for Group Database */

#include "_ARGS.h"

struct	group {
	char	*gr_name;
	char	*gr_passwd; /* Not Posix */
	short	gr_gid; /* Should be gid_t, but too hard */
	short	_gr_filler1;
	char	**gr_mem;
};

struct group *getgrgid _ARGS((gid_t _gid));
struct group *getgrnam _ARGS((char *_name));

#ifndef _POSIX_SOURCE
struct group *getgrent _ARGS((void));
#endif

#endif /* __GRP_H__ */
