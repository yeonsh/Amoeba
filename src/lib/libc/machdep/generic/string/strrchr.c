/*	@(#)strrchr.c	1.1	91/04/08 14:17:07 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strrchr - returns a pointer to the last occurrence of 'c' in 's'.
*/

char *
strrchr(s, c)
register const char *s;
register int c;
{
	register const char *result = NULL;

	c = (char) c;

	do {
		if (c == *s)
			result = s;
	} while (*s++ != '\0');

	return (char *)result;
}
