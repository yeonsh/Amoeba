/*	@(#)profile.h	1.2	94/04/06 15:43:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PROFILE_H__
#define __PROFILE_H__

struct prof {
	long *p_log;
	long p_buf[PROFILE];
};

struct profinfo {
	long pi_addr;
	long pi_size;
};

#define PRO_START	1
#define PRO_STOP	2
#define PRO_INFO	3

#endif /* __PROFILE_H__ */
