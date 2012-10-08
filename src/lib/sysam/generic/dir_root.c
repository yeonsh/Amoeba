/*	@(#)dir_root.c	1.3	96/02/27 11:21:43 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"

dir_root(cap)
capability *cap;
{
	capability *root;

	if ((root = getcap("ROOT")) == 0)
		return -1;
	*cap = *root;
	return 0;
}

#ifdef lint
char **environ;
char end;
#endif
