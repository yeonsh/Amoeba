/*	@(#)caplist.h	1.2	94/04/06 15:39:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __CAPLIST_H__
#define __CAPLIST_H__

struct caplist {
	char *cl_name;
	capability *cl_cap;
};

#endif /* __CAPLIST_H__ */
