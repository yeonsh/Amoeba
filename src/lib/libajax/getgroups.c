/*	@(#)getgroups.c	1.4	94/04/07 09:44:06 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* getgroups() POSIX 4.2.3
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"

int
getgroups(ngroups, gidset)
	int ngroups;
	gid_t *gidset;
{
	return 0;
}
