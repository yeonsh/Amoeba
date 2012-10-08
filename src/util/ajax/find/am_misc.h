/*	@(#)am_misc.h	1.3	96/02/27 13:00:14 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef AM_MISC_H
#define AM_MISC_H

/* misc definitions to get BSD find to compile on Amoeba */

#define UT_NAMESIZE     8

/* fnmatch(3) defines */
#define FNM_PATHNAME    0x01    /* match pathnames, not filenames */
#define FNM_QUOTE       0x02    /* escape special chars with \ */

#define index	strchr
#define rindex	strrchr

/* from tzfile.h: */
#define SECSPERMIN      60
#define MINSPERHOUR     60
#define HOURSPERDAY     24
#define DAYSPERNYEAR    365
#define DAYSPERLYEAR    366
#define SECSPERHOUR     (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY      ((long) SECSPERHOUR * HOURSPERDAY)

#ifdef _POSIX_SOURCE
/* then we don't have the BSD types yet: */
typedef unsigned short u_short;
typedef unsigned int u_int;

#define major(x)        (((x)>>8) & 0xff)
#define minor(x)        ((x) & 0xff)
#endif

#define	bcopy(a,b,c)	memmove(b,a,c)
#define	bzero(a,b)	memset(a, 0, b)

#endif /* AM_MISC_H */
