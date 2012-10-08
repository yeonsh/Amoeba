/*	@(#)mbtowc.c	1.1	91/04/10 10:41:06 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <stdlib.h>
#include <limits.h>
#include "ansi_compat.h"

int
mbtowc(pwc, s, n)
wchar_t *pwc;
register const char *s;
size_t n;
{
	if (s == (const char *)NULL) return 0;
	if (n <= 0) return 0;
	if (pwc) *pwc = *s;
	return (*s != 0);
}
