/*	@(#)strncpy.c	1.1	91/04/08 14:16:49 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strncpy - copy at most 'n' characters from string 's2' to 'ret'.  Null-pads
**	     'ret' if 's2' has less than 'n' characters.  'ret' won't be null-
**	     terminated if 's2' is longer than 'n' characters.
*/

char *
strncpy(ret, s2, n)
register char *ret;
register const char *s2;
register size_t	n;
{
	register char *s1 = ret;

	if (n>0) {
		while((*s1++ = *s2++) && --n > 0)
			/* EMPTY */ ;
		if ((*--s2 == '\0') && --n > 0) {
			do {
				*s1++ = '\0';
			} while(--n > 0);
		}
	}
	return ret;
}
