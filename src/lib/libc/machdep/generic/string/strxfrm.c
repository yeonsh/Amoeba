/*	@(#)strxfrm.c	1.1	91/04/08 14:17:44 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

size_t
strxfrm(s1, save, n)
register char *s1;
register const char *save;
register size_t n;
{
	register const char *s2 = save;

	while (*s2) {
		if (n > 1) {
			n--;
			*s1++ = *s2++;
		} else
			s2++;
	}
	if (n > 0)
		*s1++ = '\0';
	return s2 - save;
}
