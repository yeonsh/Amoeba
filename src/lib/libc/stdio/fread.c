/*	@(#)fread.c	1.2	94/04/07 10:52:58 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fread.c - read a number of members into an array
 */

#include	<stdio.h>

size_t
fread(ptr, size, nmemb, stream)
#ifdef __STDC__
void *ptr;
#else
char *ptr;
#endif
size_t size;
size_t nmemb;
register FILE *stream;
{
	register char *cp = ptr;
	register int c;
	size_t ndone = 0;
	register size_t s;

	if (size)
		while ( ndone < nmemb ) {
			s = size;
			do {
				if ((c = getc(stream)) != EOF)
					*cp++ = c;
				else
					return ndone;
			} while (--s);
			ndone++;
		}

	return ndone;
}
