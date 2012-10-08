/*	@(#)strcmp.c	1.1	91/04/08 14:15:43 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strcmp - return lexicographic difference between strings 's1' and 's2'.
*/

int
strcmp(s1, s2)
register const char *s1;
register const char *s2;
{
	while (*s1 == *s2++) {
		if (*s1++ == '\0') {
			return 0;
		}
	}
	if (*s1 == '\0') return -1;
	if (*--s2 == '\0') return 1;
	return *s1 - *s2;
}
