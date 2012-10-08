/*	@(#)putc.c	1.2	94/04/07 10:54:38 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * putc.c - print (or buffer) one character
 */

#include	<stdio.h>

#undef putc	/* but we still can use __putc */

int
putc(c, stream)
int c;
FILE *stream;
{
	return __putc(c, stream);
}
