/*	@(#)assert.c	1.2	94/04/07 10:46:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * assert.c - diagnostics
 */

#include	<assert.h>
#include	<stdio.h>
#include	<stdlib.h>

#ifdef __STDC__

void __bad_assertion(const char *mess)
{
	fputs(mess, stderr);
	abort();
}

#else

void __bad_assertion(file, line)
char *file;
int line;
{
	fprintf(stderr, "assertion failed: %s: %d\n", file, line);
	abort();
}

#endif /* __STDC__ */
