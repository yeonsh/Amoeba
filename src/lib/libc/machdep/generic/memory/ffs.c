/*	@(#)ffs.c	1.2	94/04/07 10:36:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** ffs - find first bit set.
**	Returns the index of the first bit set in 'i'.  Bits are
**	numbered from the right-most bit which is bit 1.
**	Returns 0 if no bit is set.
*/

int
ffs(i)
register int i;
{
    register int n;

    if (i == 0)
	return 0;
    for (n = 1; !(i & 1); n++, i >>= 1)
	/* nothing */ ;
    return n;
}
