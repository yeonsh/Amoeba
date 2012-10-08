/*	@(#)strcat.c	1.1	91/04/11 13:47:55 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* append t to s
*/
char *
strcat(s, t)
	register char *s, *t;
{
	register char *b = s;

	while (*s++)
		;
	s--;
	while (*s++ = *t++)
		;
	return b;
}
