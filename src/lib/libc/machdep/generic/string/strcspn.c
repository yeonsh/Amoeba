/*	@(#)strcspn.c	1.1	91/04/08 14:16:12 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strcspn - returns the length of the initial segment of string 'string' which
**	     contains none of the characters in string 'notin'.
*/

size_t
strcspn(string, notin)
const char *string;
const char *notin;
{
	register const char *s1, *s2;

	for (s1 = string; *s1; s1++) {
		for(s2 = notin; *s2 != *s1 && *s2; s2++)
			/* EMPTY */ ;
		if (*s2)
			break;
	}
	return s1 - string;
}
