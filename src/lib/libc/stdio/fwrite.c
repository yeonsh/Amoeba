/*	@(#)fwrite.c	1.2	94/04/07 10:53:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fwrite.c - write a number of array elements on a file
 */

#include <stdio.h>
#include "loc_incl.h"

size_t
fwrite(ptr, size, nmemb, stream)
#ifdef __STDC__
const void *ptr;
#else
char *ptr;
#endif
size_t size;
size_t nmemb;
register FILE *stream;
{
	register const unsigned char *cp = (unsigned char *)ptr;
	register size_t s;
	size_t ndone = 0;

	if (size)
		while ( ndone < nmemb ) {
			s = size;
			do {
				if (putc((int)*cp, stream) == EOF)
					return ndone;
				cp++;
			} 
			while (--s);
			ndone++;
		}
	return ndone;
}
