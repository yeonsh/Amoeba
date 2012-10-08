/*	@(#)perror.c	1.2	94/04/07 10:54:25 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * perror.c - print an error message on the standard error output
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "loc_incl.h"

void
perror(s)
const char *s;
{
	if (s && *s) {
		(void) fputs(s, stderr);
		(void) fputs(": ", stderr);
	}
	(void) fputs(strerror(errno), stderr);
	(void) fputs("\n", stderr);
}
