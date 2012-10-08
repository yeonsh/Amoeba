/*	@(#)getenv.c	1.2	94/04/07 10:48:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <string.h>
extern char **environ;

/*
 * Find name in the UNIX / Amoeba string environment, and return pointer to
 * its value.
 */
char *
getenv(name)
char *	name;
{
	char *env_lookup();

	return env_lookup(environ, name);
}
