/*	@(#)putenv.c	1.2	94/04/07 10:49:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include <stdlib.h>

unsigned int   env_alloc = 0;
extern char ** environ;

#ifndef __STDC__
#define NULL 0
#endif

char **env_put(/* char **env, unsigned int *env_allocp,
		  char *new_map, int override */);

/*
 * The arg must have the syntax "name=value", where name does not contain a
 * '='.  The (name,value) pair is added to the UNIX / Amoeba string
 * environment, where it can subsequently be looked up by getenv.  See also
 * the native Amoeba functions env_lookup and env_put.
 */
void
putenv(arg)
	char *	arg;
{
	char **env;
	if ((env = env_put(environ, &env_alloc, arg, 1)) != NULL)
		environ = env;
}
