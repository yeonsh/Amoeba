/*	@(#)fgets.c	1.2	94/04/07 10:51:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fgets.c - get a string from a file
 */

#include	<stdio.h>

char *
fgets(s, n, stream)
char *s;
register int n;
register FILE *stream;
{
	register int ch;
	register char *ptr;

	ptr = s;
	while (--n > 0 && (ch = getc(stream)) != EOF) {
		*ptr++ = ch;
		if ( ch == '\n')
			break;
	}
	if (ch == EOF) {
		if (feof(stream)) {
			if (ptr == s) return NULL;
		} else return NULL;
	}
	*ptr = '\0';
	return s;
}
