/*	@(#)strzero.c	1.2	93/01/15 17:14:55 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* strzero()
*/

char *
strzero(s)
	char *s;
{
	*s = '\0';
	return s;
}
