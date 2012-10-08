/*	@(#)strcasecmp.c	1.2	94/04/07 10:49:47 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
strcasecmp.c

Created Oct 14, 1991 by Philip Homburg
*/

#include <ctype.h>
#include <_ARGS.h>
#include <module/strmisc.h>

#ifdef __STDC__
#define _CONST	const
#else
#define _CONST
#endif

int
strcasecmp(s1, s2)
_CONST char *s1, *s2;
{
	int c1, c2;
	while (c1= toupper(*s1++), c2= toupper(*s2++), c1 == c2 && (c1 & c2))
		;
	if (c1 & c2)
		return c1 < c2 ? -1 : 1;
	return c1 ? 1 : (c2 ? -1 : 0);
}
