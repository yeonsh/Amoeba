/*	@(#)swab.c	1.2	94/04/07 10:38:22 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** swab - swaps odd with even bytes of 'from' and stores the result in 'to'.
**	  'nbytes' should be even.  For portability 'from' and 'to' should
**	  not overlap.
*/

swab(from, to, nbytes)
register char	*from;
register char	*to;
register int	nbytes;
{
    nbytes /= 2;
    while (--nbytes >= 0)
    {
	*(to+1) = *from++;
	*to = *from++;
	to += 2;
    }
}
