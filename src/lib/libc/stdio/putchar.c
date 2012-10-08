/*	@(#)putchar.c	1.2	94/04/07 10:54:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * putchar.c - print (or buffer) a character on the standard output stream
 */

#include	<stdio.h>

#undef putchar	/* but we still can use __putchar */

int
putchar(c)
int c;
{
	return __putchar(c);
}
