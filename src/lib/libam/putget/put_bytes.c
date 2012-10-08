/*	@(#)put_bytes.c	1.3	94/04/07 10:17:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
put_bytes.h

Created June 25, 1991 by Philip Homburg
*/

#include <string.h>

#include <amoeba.h>
#include <module/buffers.h>

char *
buf_put_bytes(p, endbuf, b, n)
char *	p;
char *	endbuf;
void *	b;
size_t	n;
{
	if (!p)
		return 0;
	if (p+n>endbuf)
		return 0;
	(void) memcpy(p, b, n);
	return p+n;
}
