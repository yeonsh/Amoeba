/*	@(#)strncat.c	1.1	91/04/08 14:16:30 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strncat - append at most 'n' characters of string 's2' to the end of 's1'.
*/

char *
strncat(ret, s2, n)
char *ret;
register const char *s2;
register size_t n;
{
	register char *s1 = ret;

	if (n > 0) {
		while (*s1++)
			/* EMPTY */ ;
		s1--;
		while (*s1++ = *s2++)  {
			if (--n > 0) continue;
			*s1 = '\0';
			break;
		}
		return ret;
	} else return s1;
}
