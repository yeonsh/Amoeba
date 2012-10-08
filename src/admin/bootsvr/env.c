/*	@(#)env.c	1.3	96/02/27 10:11:04 */
/*
 * Copyright 1996 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	Change the environment.
 *	Assumes the first environment is located on stack,
 *	and subsequent ones are malloc'd and must therefor
 *	be freed.
 */

#include "amoeba.h"
#include "monitor.h"
#include "stdlib.h"

#ifndef NULL
#define NULL 0
#endif

extern char **environ;

void
SetEnv(envp)
    char **envp;
{
    static int called_before = 0;
    char **oldenv;

    MON_EVENT("resetting the string environment");
    oldenv = environ;
    environ = envp;
    if (!called_before)
	called_before = 1;
    else {
	if (oldenv != NULL) {	/* Free it */
	    char **p;

	    for (p = oldenv; *p != NULL; ++p)
		free((_VOIDSTAR) *p);
	    free((_VOIDSTAR) oldenv);
	}
    }
    tzset();  /* Set the timezone if possible */
} /* SetEnv */
