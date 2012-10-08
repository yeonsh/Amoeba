/*	@(#)strlen.c	1.1	91/04/08 14:16:21 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strlen - returns number of characters in 's', exclusive the null terminator.
*/

size_t
strlen(org)
const char *org;
{
	register const char *s = org;

	while (*s++)
		/* EMPTY */ ;

	return --s - org;
}
