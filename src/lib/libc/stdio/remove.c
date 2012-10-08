/*	@(#)remove.c	1.2	94/04/07 10:54:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * remove.c - remove a file
 */

#include <stdio.h>
#include "loc_incl.h"

int unlink _ARGS((const char *path));

int
remove(filename)
const char *filename;
{
	return unlink(filename);
}
