/*	@(#)amupolicy.h	1.2	94/04/07 14:03:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __AMUPOLICY_H__
#define __AMUPOLICY_H__

/*
** Customizeable defines for the location of certain files, etc.
** on the host unix system.
*/

/* Where capabilities for amoeba hosts are kept (in .capability format) */
#define UNIX_HOST_DIR "."

/* Where bootable Amoeba kernels are kept under Unix */
#define UNIX_KERNELDIR "."

/* Where to find the users' root under unix */
#define CAP_FILENAME	".capability"	/* unix file name for Amoeba cap */

/* The pronet and ethernet host file, used by uamstat */
/* #define PRONET_HOSTS	"/etc/amoeba/hosts"	/* Comment-out if not used */
#define ETHER_HOSTS	"/etc/ethers"

#endif /* __AMUPOLICY_H__ */
