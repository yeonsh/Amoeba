/*	@(#)getw.c	1.1	91/11/21 13:44:50 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <stdio.h>

int getw(iop)
	register FILE *iop;
{
	register int cnt = sizeof(int);
	int w;
	register char *p = (char *) &w;

	while (cnt--) {
		*p++ = getc(iop);
	}
	if (feof(iop) || ferror(iop)) return EOF;
	return w;
}
