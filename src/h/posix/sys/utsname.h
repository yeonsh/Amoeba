/*	@(#)utsname.h	1.3	94/04/06 16:57:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Posix 1003.1 compliant  system identification definitions */

#ifndef __UTSNAME_H__
#define __UTSNAME_H__

/* Section 4.4 */

#include "_ARGS.h"

struct utsname {
	char sysname[32];
	char nodename[32];
	char release[32];
	char version[32];
	char machine[32];
};

int uname _ARGS((struct utsname *_name));

#endif /* __UTSNAME_H__ */
