/*	@(#)getpid.c	1.3	94/04/07 09:45:14 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* getpid() POSIX 4.1.1
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sesstuff.h"

pid_t
getpid()
{
	capability *session;
	
	SESINIT(session);
	return prv_number(&session->cap_priv);
}
