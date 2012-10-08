/*	@(#)strspn.c	1.1	91/04/08 14:17:17 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strspn - returns length of initial part of 'string' that consists entirely
**	    from characters in string 'in'.
*/

size_t
strspn(string, in)
const char *string;
const char *in;
{
	register const char *s1, *s2;

	for (s1 = string; *s1; s1++) {
		for (s2 = in; *s2 && *s2 != *s1; s2++)
			/* EMPTY */ ;
		if (*s2 == '\0')
			break;
	}
	return s1 - string;
}
