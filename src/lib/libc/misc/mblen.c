/*	@(#)mblen.c	1.1	91/04/10 10:40:36 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <stdlib.h>
#include <limits.h>
#include "ansi_compat.h"

#define	CHAR_SHIFT	8

int
mblen(s, n)
const char *s;
size_t n;
{
	if (s == (const char *)NULL) return 0; /* no state dependent codings */
	if (n <= 0) return 0;
	return (*s != 0);
}
