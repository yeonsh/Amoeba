/*	@(#)types.h	1.4	94/04/06 16:57:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Posix 1003.1 compliant type definitions */

#ifndef __TYPES_H__
#define __TYPES_H__

/* NB: Some other header files use types that should be the same as some
   defined here, but are written out as short, int or long, to avoid
   illegal dependencies between the Posix header files; if you change
   the type definitions here, you must also update those.  You can use
	grep <typename> *.h sys/?*.h
   to find those references. */

typedef short   	dev_t;
typedef int		gid_t;
typedef long		ino_t;
typedef unsigned short	mode_t;
typedef short		nlink_t;
typedef long		off_t;
typedef int		pid_t;
typedef int		uid_t;
typedef int		ssize_t;

#ifndef __STDC__

/* These two are also in <stddef.h>: */
#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef int ptrdiff_t;
#endif

/* These two typedefs are also in <time.h>: */
#ifndef _CLOCK_T
typedef long clock_t;
#define _CLOCK_T
#endif

#endif /* __STDC__ */

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _TIME_T
typedef long time_t;
#define _TIME_T
#endif

#ifndef _POSIX_SOURCE
/* For BSD: */
typedef char *		caddr_t;
typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef unsigned long	u_long;
/* For most Unixes: */
#define major(x)	(((x)>>8) & 0xff)
#define minor(x)	((x) & 0xff)
#endif

#endif /* __TYPES_H__ */
