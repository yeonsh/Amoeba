/*	@(#)setegid.c	1.2	94/04/07 09:50:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* setegid(2) system call emulation */

#include "ajax.h"

int
setegid(egid)
	int egid;
{
	setgid(egid);
}
