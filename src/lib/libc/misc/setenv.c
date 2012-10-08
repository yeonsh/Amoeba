/*	@(#)setenv.c	1.2	94/04/07 10:49:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include <stdlib.h>

extern unsigned int env_alloc;	/* Initialized in putenv.c */
extern char **      environ;

#ifndef __STDC__
#define NULL 0
#endif

char **env_put(/* char **env, unsigned int *env_allocp,
		  char *new_map, int override */);

/*
 * The (name,val) pair is added to the standard UNIX / Amoeba string
 * environment, where it can subsequently be looked up by getenv or env_lookup.
 * If override is nonzero, any existing value for name is replaced, otherwise
 * nothing happens when name already has a value.  0 is returned for success,
 * -1 for failure.  Not overriding an existing name is not a failure.  (See
 * also putenv.c and env_put.c).
 */
int
setenv(name, val, override)
	char *name, *val;
	int override;
{
	char **env;
	char *cp;
	unsigned int len_name;

	if (!name)
		return -1;

	/* Find length of name: */
	for (cp = name; *cp && *cp != '='; cp++) { }
	len_name = cp - name;
	if (len_name == 0)
		return -1;

	/* Allocate a buffer into which to construct the env_put arg of the
	 * form "name=val":
	 */
	if ((cp = (char *) malloc((len_name +
			  (size_t)strlen(val)) * sizeof(char) + 2)) == NULL)
		return -1;
	(void) strncpy(cp, name, len_name);
	*(cp + len_name) = '=';
	(void) strcpy(cp + len_name + 1, val);

	if ((env = env_put(environ, &env_alloc, cp, override)) != NULL) {
		environ = env;
		return 0;
	} else
		return -1;
}

/*
 * Remove a name from the environment.  See also env_delete.c.
 */
void
unsetenv(name)
	char	*name;
{
	extern void env_delete(/* char **env, char *name */);
	env_delete(environ, name);
}
