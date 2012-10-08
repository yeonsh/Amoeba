/*	@(#)env_put.c	1.2	94/04/07 10:02:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <string.h>

#ifndef __STDC__
#ifndef NULL
#define NULL	0
#endif
#endif

/*
 * This is the native Amoeba function for modifying a string environment,
 * which is just a mapping from character string names to character string
 * values.
 *
 * It can operate on either the standard environment pointed to by the
 * global "environ" variable, or on any other environment with the same
 * structure.  It is called by the UNIX-emulation functions putenv
 * and setenv, which always operate on "environ".
 *
 * Env should point to a NULL-terminated array of pointers to NUL-terminated
 * character strings.  Each string should have the form "name=value" (where
 * "name" does not contain '='), specifying that "name" maps to "value".  No
 * two strings in the same environment should use the same name.
 *
 * Env_alloc should be a pointer to an int containing the number of elements
 * allocated (not necessarily in use) in the env array.  If the array is not
 * a malloc'd block of storage, this must be set to zero, regardless of the
 * number of array elements in use.
 *
 * Newmap should also be of the form "name=value".  It specifies a new
 * mapping to be added to the environment, or to replace any existing
 * mapping for "name".  Override must be non-zero if any existing mapping
 * is to be replaced.
 *
 * Env_put returns a pointer to the new environment on success (which
 * includes refusing to replace an existing name), and NULL on failure,
 * such as bad environment or malloc failure.
 *
 * On success, it will have realloc'd the env array to be larger, if it
 * was full, except that if *env_alloc==0, it will use malloc/copy
 * instead of realloc.  In either case, it will update the int pointed to by
 * env_alloc to indicate the final allocated size of the env array that is
 * returned.
 */
char **
env_put(env, env_alloc, newmap, override)
	char **       env;		/* Array of pointers to strings */
	unsigned int *env_alloc;	/* Alloc'd size of the env array */
	char *	      newmap;		/* String of the form "name=value" */
	int	      override;		/* Replace any existing val for name */
{
	int env_slots_in_use;
	char *endnewname, *endoldname;
	register char **ep;

	if ((endnewname = strchr(newmap, '=')) == NULL || endnewname == newmap)
		return NULL;	/* Bad syntax */

	/* Check whether it already exists in env: */
	for (ep = env; *ep; ep++) {
	    if ((endoldname = strchr(*ep, '=')) == NULL || endoldname == *ep)
		continue;	/* Bad syntax in env string! */
	    else if (endoldname - *ep == endnewname - newmap &&
			      strncmp(*ep, newmap,
				      (unsigned int)(endnewname-newmap)) == 0) {
		/* The name is already in env.  Update it, if override set: */
		if (override)
			*ep = newmap;
		return env;
	    }
	}
	env_slots_in_use = ep - env;  /* Not counting NULL terminator */

	/* The name is not currently in the env, extend it: */
	if (env_slots_in_use + 1 >= *env_alloc) {
		char **new_environ;
		unsigned int new_slots_allocd;
		if (*env_alloc == 0) {
			new_slots_allocd = env_slots_in_use < 5 ? 10 :
						(env_slots_in_use + 1) * 2;
			new_environ = (char **)
				     malloc(new_slots_allocd * sizeof(char *));
			if (new_environ) {
				char **np;
				for (ep=env, np=new_environ; *ep; ep++, np++)
					*np = *ep;
				*np = NULL;
			}
		} else {
			new_slots_allocd = *env_alloc * 2;
			new_environ = (char **)realloc((char *)env,
					    new_slots_allocd * sizeof(char *));
		}
		if (!new_environ)
			return NULL;
		*env_alloc = new_slots_allocd;
		env = new_environ;
	}
	env[env_slots_in_use] = newmap;
	env[env_slots_in_use + 1] = NULL;
	return env;
}
