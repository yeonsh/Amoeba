/*	@(#)utime.h	1.2	94/04/06 16:54:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __UTIME_H__
#define __UTIME_H__

/* Posix <utime.h> */

/* Requires <sys/types.h> */

struct utimbuf {
	time_t actime;
	time_t modtime;
};

#endif /* __UTIME_H__ */
