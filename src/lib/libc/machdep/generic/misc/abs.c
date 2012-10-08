/*	@(#)abs.c	1.2	94/04/07 10:37:43 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** abs - return the absolute value of 'i'.  Probably a bug when i is most
**	 negative integer possible.
*/

int
abs(i)
int	i;
{
    return i < 0 ? -i : i;
}
