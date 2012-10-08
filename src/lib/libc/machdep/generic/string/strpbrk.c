/*	@(#)strpbrk.c	1.1	91/04/08 14:16:58 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strpbrk - returns a pointer to first occurrence in 'string' of any
**	     character in 'brk', or a null-pointer if none are found.
*/

char *
strpbrk(string, brk)
register const char *string;
register const char *brk;
{
	register const char *s1;

	while (*string) {
		for (s1 = brk; *s1 && *s1 != *string; s1++)
			/* EMPTY */ ;
		if (*s1)
			return (char *)string;
		string++;
	}
	return (char *)NULL;
}
