/*	@(#)env_lookup.c	1.2	94/04/07 10:02:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>

/*
 * This is the native Amoeba function for looking up the value of a name in
 * a string environment, which is just a mapping from character string names
 * to character string values.
 *
 * It can operate on either the standard environment pointed to by the
 * global "environ" variable, or on any other environment with the same
 * structure.  It is called by the UNIX-emulation function getenv, which
 * always operates on "environ".
 *
 * Env should point to a NULL-terminated array of pointers to NUL-terminated
 * character strings.  Each string should have the form "name=value" (where
 * "name" does not contain '='), specifying that "name" maps to "value".  No
 * two strings in the same environment should use the same name.
 *
 * Env_lookup returns a pointer to the value for the specified name, or NULL
 * if the name is not found in the environment.
 */
char *
env_lookup(env, name)
char ** env;
char *	name;
{
	char **ep, *endoldname, *cp;
	unsigned int len_name;

	if (!name)
		return (char *)0;

	/* Find length of search name: */
	for (cp = name; *cp && *cp != '='; cp++) { }
	len_name = cp - name;
	if (len_name == 0)
		return (char *)0;

	/* Check whether it already exists in env: */
	for (ep = env; *ep; ep++) {
	    if ((endoldname = strchr(*ep, '=')) == 0 || endoldname == *ep)
		continue;	/* Bad syntax in env string! */
	    else if (endoldname - *ep == len_name &&
				    strncmp(*ep, name, len_name) == 0) {
		return endoldname + 1;
	    }
	}
	return (char *)0;
}
