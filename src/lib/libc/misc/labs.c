/*	@(#)labs.c	1.1	91/04/10 10:39:45 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <stdlib.h>

long
labs(l)
register long l;
{
	return l >= 0 ? l : -l;
}
