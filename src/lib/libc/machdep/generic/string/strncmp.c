/*	@(#)strncmp.c	1.1	91/04/08 14:16:39 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strncmp - return lexicographic difference between 's1' and 's2', but stop
**	     after 'n' characters.
*/

int
strncmp(s1, s2, n)
register const char *s1;
register const char *s2;
register size_t      n;
{
	if (n) {
		do {
			if (*s1 != *s2++)
				break;
			if (*s1++ == '\0')
				return 0;
		} while (--n > 0);
		if (n > 0) {
			if (*s1 == '\0') return -1;
			if (*--s2 == '\0') return 1;
			return *s1 - *s2;
		}
	}
	return 0;
}
