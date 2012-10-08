/*	@(#)env_delete.c	1.2	94/04/07 10:02:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>

#ifndef __STDC__
#define NULL 0
#endif

/*
 * This is the native Amoeba function for deleting a name from
 * a string environment, which is just a mapping from character string names
 * to character string values.
 *
 * It can operate on either the standard environment pointed to by the
 * global "environ" variable, or on any other environment with the same
 * structure.  It is called by the UNIX-emulation function unsetenv, which
 * always operates on "environ".
 *
 * Env should point to a NULL-terminated array of pointers to NUL-terminated
 * character strings.  Each string should have the form "name=value" (where
 * "name" does not contain '='), specifying that "name" maps to "value".  No
 * two strings in the same environment should use the same name.
 */
void
env_delete(env, name)
char ** env;
char *	name;
{
 	char **ep, **np, *cp, *endoldname;
 	unsigned int len_name;
 
 	if (!name)
 		return;
 
 	/* Find length of name: */
 	for (cp = name; *cp && *cp != '='; cp++) { }
 	len_name = cp - name;
 	if (len_name == 0)
 		return;
 
 	/* Delete all entries with name from the environment, copying the
 	 * subsequent entries down to fill the gap:
 	 */
 	for (np = ep = env; (*np = *ep) != NULL; ep++, np++) {
 	    if ((endoldname = strchr(*ep, '=')) == 0 || endoldname == *ep)
 		continue;	/* Bad syntax in env string! */
 	    else if (endoldname - *ep == len_name &&
 				    strncmp(*ep, name, len_name) == 0)
 		--np;
 	}
}
