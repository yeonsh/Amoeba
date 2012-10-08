/*	@(#)exec_fndhost.h	1.3	94/04/06 15:40:42 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __EXEC_FINDHOST_H__
#define __EXEC_FINDHOST_H__

/* Error codes returned be exec_findhost() and exec_multi_findhost().
 * Must be negative.
 */

#define FPE_BADARCH	(RUN_FIRST_ERR - 1)   /* Unknown architecture */
#define FPE_NOPOOLDIR	(RUN_FIRST_ERR - 2)   /* Can't find pool directory */
#define FPE_BADPOOLDIR	(RUN_FIRST_ERR - 3)   /* Can't open pool directory */
#define FPE_NONE	(RUN_FIRST_ERR - 4)   /* No suitable host in pooldir */
#define FPE_DOWN	(RUN_FIRST_ERR - 5)   /* Can't open host's proc cap */

#define PD_HOSTSIZE	16	/* maximum size of a host name */

#endif /* __EXEC_FINDHOST_H__ */
