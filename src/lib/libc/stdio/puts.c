/*	@(#)puts.c	1.2	94/04/07 10:54:50 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * puts.c - print a string onto the standard output stream
 */

#include <stdio.h>
#include "loc_incl.h"

int
puts(s)
register const char *s;
{
	register FILE *file = stdout;
	register int i = 0;

	while (*s) {
		if (putc(*s++, file) == EOF) return EOF;
		else i++;
	}
	if (putc('\n', file) == EOF) return EOF;
	return i + 1;
}
