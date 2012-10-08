/*	@(#)strcat.c	1.1	91/04/08 14:15:22 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strcat - append string 's2' to 's1'.
*/

char *
strcat(ret, s2)
register char	    *ret;
register const char *s2;
{
	register char *s1 = ret;

	while (*s1++ != '\0')
		/* EMPTY */ ;
	s1--;
	while (*s1++ = *s2++)
		/* EMPTY */ ;
	return ret;
}
