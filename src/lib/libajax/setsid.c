/*	@(#)setsid.c	1.2	94/04/07 09:51:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* setsid() POSIX 4.3.2
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include <sys/types.h>

pid_t 
setsid()
{
	ERR(ENOSYS, "setsid: not supported");
}
