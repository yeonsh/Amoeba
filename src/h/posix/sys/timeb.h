/*	@(#)timeb.h	1.2	94/04/06 16:57:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SYS_TIMEB_H__
#define __SYS_TIMEB_H__

/* Not POSIX compliant, but currently needed to compile libamoeba under
 * Amoeba.
 */

/*
 * Structure returned by ftime system call
 */
struct timeb
{
	time_t		time;
	unsigned short	millitm;
	short		timezone;
	short		dstflag;
};

#endif /* __SYS_TIMEB_H__ */
