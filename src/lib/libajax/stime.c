/*	@(#)stime.c	1.2	94/04/07 09:53:18 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* stime(2) system call emulation (always fails) */

#include "ajax.h"

/* ARGSUSED */
int
stime(tp)
	long *tp;
{
	ERR(EPERM, "stime: not super user");
}
