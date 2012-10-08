/*	@(#)mknod.c	1.2	94/04/07 09:49:03 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* mknod(2) system call emulation (always fails) */

#include "ajax.h"

/*ARGSUSED*/
int
mknod(path, mode, dev)
	char *path;
	int mode;
	int dev;
{
	ERR(EPERM, "mknod: not implemented");
}
