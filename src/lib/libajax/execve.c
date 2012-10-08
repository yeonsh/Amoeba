/*	@(#)execve.c	1.2	94/04/07 09:43:10 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* execve(2) system call emulation */

#include "ajax.h"

int
execve(file, argv, envp)
	char *file;
	char **argv;
	char **envp;
{
	return _ajax_startproc(0, file, argv, envp, -1, (int *)NULL, -1L);
}
