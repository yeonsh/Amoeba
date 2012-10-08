/*	@(#)portstat.h	1.2	94/04/06 15:42:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PORTSTAT_H__
#define __PORTSTAT_H__

struct portstat {
	long pts_alloc;
	long pts_aged;
	long pts_full;
	long pts_wakeup;
	long pts_here;
	long pts_lookup;
	long pts_flocal;
	long pts_fglobal;
	long pts_portask;
	long pts_portyes;
	long pts_locate;
	long pts_nolocate;
	long pts_relocate;
};

#endif /* __PORTSTAT_H__ */
