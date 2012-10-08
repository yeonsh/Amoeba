/*	@(#)lainfo.h	1.2	94/04/06 16:44:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __LAINFO_H__
#define __LAINFO_H__

/*
 * parameters for lance driver
 */

extern
struct lainfo {
	char *	  (*lai_alloc)();
	vir_bytes lai_devaddr;
	intrinfo  lai_intrinfo;
	short	  lai_csr3;
} lainfo;

#endif /* __LAINFO_H__ */
