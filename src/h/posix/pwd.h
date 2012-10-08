/*	@(#)pwd.h	1.4	96/02/27 10:34:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PWD_H__
#define __PWD_H__

/* Posix 1003.1 compliant definitions for User Database */

#include "_ARGS.h"

struct	passwd {
	/*
	** This structure lay-out is inspired by v7/BSD compatibility,
	** plus what getpwent.c actually implements.
	*/
	char	*pw_name;
	char	*pw_passwd;	/* Not Posix, but set by getpwent() */
	int	pw_uid;		/* Should be uid_t, but too hard */
	int	pw_gid;		/* Should be gid_t, but too hard */
	int	_pw_filler1;	/* Not Posix (BSD pw_quota) */
	long	_pw_filler2;	/* Not Posix (BSD pw_comment */
	char	*pw_gecos;	/* Not Posix, but set by getpwent() */
	char	*pw_dir;
	char	*pw_shell;
};

struct passwd *getpwuid _ARGS((int _uid));
struct passwd *getpwnam _ARGS((char *_name));

#ifndef _POSIX_SOURCE
struct passwd *getpwent _ARGS((void));
#endif

#endif /* __PWD_H__ */
