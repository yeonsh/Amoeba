/*	@(#)memccpy.c	1.2	94/04/07 10:36:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** memccpy - copy characters from 'src' to 'dst' until either 'n' chars have
**	     been copied or the character 'c' is encountered.  Return pointer
**	     to character after 'c' or null pointer if 'c' wasn't found.
*/

char *
memccpy(dst, src, c, n)
register char	*dst;
register char	*src;
register int 	c;
register int 	n;
{
    while (--n >= 0)
	if ((*dst++ = *src++) == c)
	    return dst;
    return 0;
}
