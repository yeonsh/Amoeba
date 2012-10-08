/*	@(#)strstr.c	1.1	91/04/08 14:17:26 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strstr - return a pointer to the first occurrence of the string 'wanted'
**	    in the string 's'.
*/

char *
strstr(s, wanted)
register const char *s;
register const char *wanted;
{
	register const int len = strlen(wanted);

	if (len == 0) return (char *)s;
	while (*s != *wanted || strncmp(s, wanted, len))
		if (*s++ == '\0')
			return (char *)NULL;
	return (char *)s;
}
