/*	@(#)execv.c	1.2	94/04/07 09:43:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* execv(3) library call emulation */

#include "ajax.h"

extern char **environ;

int
execv(name, argv)
	char *name;
	char **argv;
{
	return execve(name, argv, environ);
}
