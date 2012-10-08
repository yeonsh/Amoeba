/*	@(#)sethostent.c	1.1	91/11/19 09:58:50 */
/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)sethostent.c	6.6 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

#include <stddef.h>

#include <_ARGS.h>
#include <server/ip/types.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/nameser.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/resolv.h>

void sethostent(stayopen)
{
	if (stayopen)
		_res.options |= RES_STAYOPEN | RES_USEVC;
}

void endhostent()
{
	_res.options &= ~(RES_STAYOPEN | RES_USEVC);
	_res_close();
}

void sethostfile(name)
char *name;
{
#ifdef lint
name = name;
#endif
}
