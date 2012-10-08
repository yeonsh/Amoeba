/*	@(#)times.h	1.3	94/04/06 16:57:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Posix 1003.1 compliant times() interface */

#ifndef __TIMES_H__
#define __TIMES_H__

#include "_ARGS.h"

/* 4.5.2 Process Times */

#ifndef _CLOCK_T
typedef long clock_t;
#define _CLOCK_T
#endif

struct tms {
	clock_t tms_utime;	/* User CPU time */
	clock_t tms_stime;	/* System CPU time */
	clock_t tms_cutime;	/* User CPU time of terminated children */
	clock_t tms_cstime;	/* System CPU time of terminated children */
};

/* All times are in {CLK_TCK}ths of a second */

clock_t times _ARGS((struct tms *_buffer));

#endif /* __TIMES_H__ */
