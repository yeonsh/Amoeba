/*	@(#)mbstowcs.c	1.1	91/04/10 10:40:48 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <stdlib.h>
#include "ansi_compat.h"

size_t
mbstowcs(pwcs, s, n)
register wchar_t *pwcs;
register const char *s;
size_t n;
{
	register int i = n;

	while (--i >= 0) {
		if (!(*pwcs++ = *s++))
			return n - i - 1;
	}
	return n - i;
}

