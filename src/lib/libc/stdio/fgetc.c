/*	@(#)fgetc.c	1.2	94/04/07 10:51:41 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fgetc - get an unsigned character and return it as an int
 */

#include	<stdio.h>

int
fgetc(stream)
FILE *stream;
{
	return getc(stream);
}
