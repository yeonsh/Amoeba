/*	@(#)strcpy.c	1.1	91/04/08 14:16:02 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strcpy - copy string 's2' to 's1'
*/

char *
strcpy(ret, s2)
register char	    *ret;
register const char *s2;
{
	register char *s1 = ret;

	while (*s1++ = *s2++)
		/* EMPTY */ ;

	return ret;
}
