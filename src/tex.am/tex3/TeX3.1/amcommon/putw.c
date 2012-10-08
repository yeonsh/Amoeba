/*	@(#)putw.c	1.1	91/11/21 13:45:08 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <stdio.h>

int
putw(w, iop)
	register FILE *iop;
{
	register int cnt = sizeof(int);
	register char *p = (char *) &w;

	while (cnt--) {
		putc(*p++, iop);
	}
	if (ferror(iop)) return EOF;
	return w;
}
