/*	@(#)strcoll.c	1.1	91/04/08 14:15:52 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include <locale.h>
#include "ansi_compat.h"

int
strcoll(s1, s2)
register const char *s1;
register const char *s2;
{
	while (*s1 == *s2++) {
		if (*s1++ == '\0') {
			return 0;
		}
	}
	return *s1 - *--s2;
}
