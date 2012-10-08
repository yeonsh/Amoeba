/*	@(#)memset.c	1.3	94/04/07 10:37:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * memset - sets first 'n' characters of memory beginning at 's' to 'c'.
 *	    Returns 's';
 *
 *	Does 4 bytes at a time if it looks like offering a reasonable
 *	performance gain.  This code is 32-bit machine specific, I fear.
 *
 * Author: Gregory J. Sharp, Feb 1993
 */

#include <string.h>

#ifndef __STDC__
#define const /**/
#endif

_VOIDSTAR
memset(s, c, n)
_VOIDSTAR	s;
register int	c;
register size_t	n;
{
    register char *s1 = (char *) s;
    register size_t x;

    if (n > 20) /* Only do 4 byte at a time version if it's worth it */
    {
	/* Make sure we are 4 byte aligned */
	while ((unsigned long) s1 & 3)
	{
	    *s1++ = c;
	    n--;
	}

	/* Put a copy of the character in all 4 bytes of c */
	c = (c << 8) | c;
	c = (c << 16) | c;
	x = (n >> 2);

	do
	{
	    *(unsigned long *) s1 = c;
	    s1 += 4;
	} while (--x != 0);
	/* Should be less than 4 bytes to do now */
	n &= 3;
    }
    /* Handle any remaining bytes */
    if (n > 0)
    {
	do
	{
	    *s1++ = c;
	} while (--n != 0);
    }
    return s;
}
