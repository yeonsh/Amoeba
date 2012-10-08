/*	@(#)symlink.c	1.2	94/04/07 09:53:26 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* symlink(2) system call emulation */

#include "ajax.h"

int
symlink(file1, file2)
	char *file1;
	char *file2;
{
	return link(file1, file2);
}
