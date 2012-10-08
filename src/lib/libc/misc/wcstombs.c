/*	@(#)wcstombs.c	1.1	91/04/10 10:44:03 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <stdlib.h>
#include <locale.h>
#include <limits.h>
#include "ansi_compat.h"

size_t
wcstombs(s, pwcs, n)
register char *s;
register const wchar_t *pwcs;
size_t n;
{
	register int i = n;

	while (--i >= 0) {
		if (!(*s++ = *pwcs++))
			break;
	}
	return n - i - 1;
}
