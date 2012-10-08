/*	@(#)setbuf.c	1.2	94/04/07 10:55:22 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * setbuf.c - control buffering of a stream
 */

#include	<stdio.h>
#include	"loc_incl.h"

void
setbuf(stream, buf)
register FILE *stream;
char *buf;
{
	(void) setvbuf(stream, buf, (buf ? _IOFBF : _IONBF), (size_t) BUFSIZ);
}
