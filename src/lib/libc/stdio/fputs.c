/*	@(#)fputs.c	1.2	94/04/07 10:52:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fputs - print a string
 */

#include	<stdio.h>
#include	"loc_incl.h"

int
fputs(s, stream)
register const char *s;
register FILE *stream;
{
	register int i = 0;

	while (*s) 
		if (putc(*s++, stream) == EOF) return EOF;
		else i++;

	return i;
}
