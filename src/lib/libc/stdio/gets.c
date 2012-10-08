/*	@(#)gets.c	1.2	94/04/07 10:53:50 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * gets.c - read a line from a stream
 */

#include	<stdio.h>

char *
gets(s)
char *s;
{
	register FILE *stream = stdin;
	register int ch;
	register char *ptr;

	ptr = s;
	while ((ch = getc(stream)) != EOF && ch != '\n')
		*ptr++ = ch;

	if (ch == EOF) {
		if (feof(stream)) {
			if (ptr == s) return NULL;
		} else return NULL;
	}

	*ptr = '\0';
	return s;
}
