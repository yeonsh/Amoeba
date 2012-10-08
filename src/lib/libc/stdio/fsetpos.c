/*	@(#)fsetpos.c	1.2	94/04/07 10:53:21 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fsetpos.c - set the position in the file
 */

#include	<stdio.h>

int
fsetpos(stream, pos)
FILE *stream;
fpos_t *pos;
{
	return fseek(stream, *pos, SEEK_SET);
}
