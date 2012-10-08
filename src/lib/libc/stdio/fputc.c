/*	@(#)fputc.c	1.2	94/04/07 10:52:45 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fputc.c - print an unsigned character
 */

#include	<stdio.h>

int
fputc(c, stream)
int c;
FILE *stream;
{
	return putc(c, stream);
}
