/*	@(#)strchr.c	1.1	91/04/08 14:15:31 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

/*
** strchr - return pointer to first occurrence of character 'c' in string 's'.
*/

#include <string.h>
#include "ansi_compat.h"

char *
strchr(s, c)
register const char *s;
register int c;
{
	c = (char) c;

	while (c != *s) {
		if (*s++ == '\0') return NULL;
	}
	return (char *)s;
}
