/*	@(#)memchr.c	1.2	94/04/07 10:37:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** memchr - returns pointer to first occurrence of character 'c' in first 'n'
**          characters of string 's'.  Returns null pointer if 'c' wasn't found.
*/

#include <string.h>

#ifndef __STDC__
#define const /*const*/
#endif

_VOIDSTAR
memchr(s, c, n)
const _VOIDSTAR	s;
register int	c;
register size_t	n;
{
	register const unsigned char *s1 = (unsigned char *)s;

	c = (unsigned char) c;
	if (n) {
		n++;
		while (--n > 0) {
			if (*s1++ != c) continue;
			return (_VOIDSTAR) --s1;
		}
	}
	return NULL;
}

