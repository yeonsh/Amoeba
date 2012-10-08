/*	@(#)wctomb.c	1.1	91/04/10 10:44:13 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include	<stdlib.h>
#include	<limits.h>

#ifdef __STDC__
int wctomb(char *s, wchar_t wchar)
#else
int wctomb(s, wchar) char *s; wchar_t wchar;
#endif
{
	if (!s) return 0;		/* no state dependent codings */

	*s = wchar;
	return 1;
}
